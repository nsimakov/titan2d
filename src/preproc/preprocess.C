/*******************************************************************
 * Copyright (C) 2003 University at Buffalo
 *
 * This software can be redistributed free of charge.  See COPYING
 * file in the top distribution directory for more details.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Author: 
 * Description: 
 *
 *******************************************************************
 * $Id: preprocess.C 233 2012-03-27 18:30:40Z dkumar $ 
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include<iostream>
#include<fstream>
using namespace std;

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "boundary_preproc.h"
#include "element_preproc.h"
#include "../header/FileFormat.h"
#include "../header/titan_simulation.h"
#include "node_preproc.h"
#include "useful_lib.h"

#include "../header/properties.h"
#include "preproc.h"

//load has to be applied on the middle node of the face!!!
/* executable must include the 3, 6, 7 or 10 arguments on the command line, they are 
 1) the number of processors -- e.g. ./preprocess 1 ...   for 1 processor!
 2) the number of cells in the y direction
 3) the full path of the GIS database   (GRASS FORMAT ONLY)
 4) the location                        (GRASS FORMAT ONLY)
 5) the mapset                          (GRASS FORMAT ONLY)
 6) the raster map name                 (GDAL FORMAT/GRASS FORMAT)
 7) (optional) the requested minimum x  
 8) (optional) the requested minimum y
 9) (optional) the requested maximum x
 10) (optional) the requested maximum y
 all or none of the optional arguments must be present */

//createfunky() is found in createfunky.C
/*void createfunky(int NumProc, int gisformat, char *GISDbase, char *location, char *mapset,
 char *topomap, int havelimits, double limits[4], int *node_count, NodePreproc **node,
 int *element_count, ElementPreproc **element, int *force_count, int *constraint_count,
 BoundaryPreproc **boundary, int *material_count, char ***materialnames, double **lambda, double **mu);
 */
int Read_no_of_objects(int*, int*, int*, int*, int*, long*);
void Read_node_data(int*, NodePreproc*, long*);
void Read_element_data(int*, NodePreproc*, ElementPreproc*, long*);
void Read_boundary_data(int*, int*, NodePreproc*, BoundaryPreproc*, long*);
void Write_data(int, int, int, int, int, NodePreproc*, ElementPreproc**, BoundaryPreproc*, unsigned*, unsigned*,
                double*, double*, char**, double*, double*);
void Determine_neighbors(int, ElementPreproc*, int, NodePreproc*);

const int material_length = 80;

int compare_key_fn(const void* elem1, const void* elem2)
{
    ElementPreproc** em1 = (ElementPreproc**) elem1;
    ElementPreproc** em2 = (ElementPreproc**) elem2;
    if(*((*em1)->pass_key()) < *((*em2)->pass_key()))
        return (-1);
    else if(*((*em1)->pass_key()) > *((*em2)->pass_key()))
        return (1);
    else if(*((*em1)->pass_key() + 1) < *((*em2)->pass_key() + 1))
        return (-1);
    else if(*((*em1)->pass_key() + 1) > *((*em2)->pass_key() + 1))
        return (1);
    else if(*((*em1)->pass_key() + 1) == *((*em2)->pass_key() + 1))
        return (0);
    
    printf("something wrong in the qsort function compare key!\n");
    return (0);
}

TitanPreproc::TitanPreproc(cxxTitanSimulation *tSim)
{
    NumProc = -1;
    gis_format = -1;
    topomain = "";
    toposub = "";
    topomapset = "";
    topomap = "";
    
    min_location_x = 0.0;
    max_location_x = 0.0;
    min_location_y = 0.0;
    max_location_y = 0.0;
    
    region_limits_set = true;
    
    NumProc = tSim->numprocs;
    
    MapNames* mapnames_ptr=tSim->get_mapnames();
    gis_format = mapnames_ptr->gis_format;
    topomain = mapnames_ptr->gis_main;
    toposub = mapnames_ptr->gis_sub;
    topomapset = mapnames_ptr->gis_mapset;
    topomap = mapnames_ptr->gis_map;

    //material_map = tSim->material_map;
    matprops=tSim->get_matprops();
    
    integrator=tSim->integrator;

    min_location_x = mapnames_ptr->min_location_x;
    max_location_x = mapnames_ptr->max_location_x;
    min_location_y = mapnames_ptr->min_location_y;
    max_location_y = mapnames_ptr->max_location_y;
    
    region_limits_set = mapnames_ptr->region_limits_set;
}
TitanPreproc::~TitanPreproc()
{
    
}

bool TitanPreproc::validate()
{
    return true;
}

void TitanPreproc::run()
{
    char **materialnames;
    unsigned nkey = 2;
    unsigned minkey[2] =
    { 0, 0 };
    unsigned maxkey[2] =
    { 0, 0 };
    int i; //generic indice
    int node_count, element_count, force_count, constraint_count, material_count;
    long location; /* "current" location within the intermediate file (funky.bin
     or funky.dat), this is only used if you're reading from 
     an intermediate file (i.e. you're NOT passing the funky 
     directly from createfunky() to main()) see FileFormat.h */
    //int NumProc; //the number of processes
    int ny; /* either the number of initial cells or gridpoints in y direction.
     the old documentation says gridpoints but code looks like cells 
     see createfunky.C */
    int havelimits; /* flag to say if optional arguments i.e. limits were passed
     in */
    double limits[4]; /* optional (string) arguments of main() are requested 
     xmin, ymin, xmax, ymax (which are doubles) */
    double *lambda, *mu; //internal and bed friction angles
    double max[2] =
    { 0, 0 };
    double min[2] =
    { 0, 0 };
    
    NodePreproc *node;
    ElementPreproc *element, **ordering;
    BoundaryPreproc *boundary;
    
    //NumProc = atoi(argv[1]); // the number of processors -- this parameter is passed in
    //int gis_format = atoi(argv[3]);
    
    if(region_limits_set)
    {
        havelimits = 1;
        limits[0] = min_location_x;
        limits[1] = max_location_x;
        limits[2] = min_location_y;
        limits[3] = max_location_y;
    }
    else
        havelimits = 0;
    
    // gis-stuff
    char *gisdb, *gisloc, *gismapset, *mapname;
    // copy file structure of data is in grass format
    if(gis_format == 1)
    {
        gisdb = strdup(topomain.c_str());
        gisloc = strdup(toposub.c_str());
        gismapset = strdup(topomapset.c_str());
        mapname = strdup(topomap.c_str());
    }
    else
    {
        gisloc = NULL;
        gismapset = NULL;
        mapname = strdup(topomap.c_str());
    }
    createfunky(limits, &node_count, &node, &element_count, &element, &force_count, &constraint_count, &boundary,
                &material_count, &materialnames, &lambda, &mu);
    
    ordering = (ElementPreproc **) calloc(element_count, sizeof(ElementPreproc*));
    element[0].create_m_node(max, min);
    
    min[0] = max[0] = *((*(element[0].get_element_node()))->get_node_coord());
    min[1] = max[1] = *((*(element[0].get_element_node()))->get_node_coord() + 1);
    
    for(i = 0; i < element_count; i++)
        element[i].create_m_node(max, min);
    
    Determine_neighbors(element_count, element, node_count, node);
    
    for(i = 0; i < node_count; i++)
        node[i].determine_max_min(max, min);
    
    node[0].determine_the_key(nkey, max, min, maxkey, minkey);
    for(i = 0; i < 2; i++)
        maxkey[i] = minkey[i] = *(node[0].get_key() + i);
    
    for(i = 1; i < node_count; i++)
        node[i].determine_the_key(nkey, max, min, maxkey, minkey);
    
    for(i = 0; i < element_count; i++)
        (*(element[i].get_element_node() + 8))->determine_the_key(nkey, max, min, maxkey, minkey);
    
    for(i = 0; i < element_count; i++)
        ordering[i] = &(element[i]);
    
    // before doing qsort, switch the first element with the middle element
    ElementPreproc* EmTemp = ordering[0];
    ordering[0] = ordering[element_count / 2];
    ordering[element_count / 2] = EmTemp;
    
    qsort(ordering, element_count, sizeof(ElementPreproc*), compare_key_fn);
    
    for(i = 0; i < element_count; i++)
        (ordering[i])->myproc(NumProc, i, element_count);
    
    Write_data(NumProc, node_count, element_count, (force_count + constraint_count), material_count, node, ordering,
               boundary, maxkey, minkey, min, max, materialnames, lambda, mu);
    
    CDeAllocD1(lambda); //see useful_lib.h
    CDeAllocD1(mu);
    
    for(int imat = 1; imat <= material_count; imat++)
        free(materialnames[imat]);
    free(materialnames);
    
    free(node);
    free(element);
    free(boundary);
    free(ordering);
    return;
    
}

//*****************************FUNCTIONS*******************************

//****************************DATA READ IN*****************************

int Read_no_of_objects(int* nc, int* ec, int* fc, int* cc, int* mc, long* loc)
{
#ifdef READFUNKYBIN
    int version;
    FILE *fp=fopen_bin("funky.bin","r");
#ifdef DEBUGFUNKYBIN
    FILE *fpD=fopen("funky.bin.debug","w");
#endif
    if(!fp)
    {   printf("file not found\n"); return(1);}
    freadI(fp,&version);
    if((version==20030722)||(version==20030802))
    {   
        freadI(fp,nc); //get the number of nodes
        freadI(fp,ec);//get the number of elements
        freadI(fp,cc);//get the number of essential bc's, the constraint count
        freadI(fp,fc);//get the number of natural bc's, the force count
        freadI(fp,mc);//get the number of materials
        *loc=ftell(fp);//location in the binary file
        fclose(fp);
#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%8d\n",*nc);
        fprintf(fpD,"%8d\n",*ec);
        fprintf(fpD,"%8d\n",*cc);
        fprintf(fpD,"%8d\n",*fc);
        fprintf(fpD,"%8d\n",*mc);
        fclose(fpD);
#endif
        // printf("done\n"); fflush(stdout);
        return(0);}
    else
    {   
        printf("Read_no_of_objects() doesn't recognize version %d\n",version);
        fclose(fp);
        return(1);}
#else
    char endline;
    ifstream inDatafile("funky.dat", ios::in);
    if(inDatafile.fail())
    {
        cout << "file not found\n";
        return (1);
    }
    inDatafile >> *nc;
    endline = '1';
    while (endline != '\n')
        inDatafile.get(endline);
    inDatafile >> *ec;
    endline = '1';
    while (endline != '\n')
        inDatafile.get(endline);
    inDatafile >> *cc;
    endline = '1';
    while (endline != '\n')
        inDatafile.get(endline);
    inDatafile >> *fc;
    endline = '1';
    while (endline != '\n')
        inDatafile.get(endline);
    inDatafile >> *mc;
    endline = '1';
    while (endline != '\n')
        inDatafile.get(endline);
    
    *loc = inDatafile.tellg();
    inDatafile.close();
    // cout<<"done"<<flush;
    return (0);
#endif
    
}

void Read_node_data(int* nc, NodePreproc n[], long* loc)
{
    int node_id, i;
    double node_coordinates[2];
#ifdef BINARYPRE
    FILE *fp=fopen_bin("funky.bin","r");
    int version;
#ifdef DEBUGFUNKYBIN
    FILE *fpD=fopen("funky.bin.debug","a");
#endif
    if(!fp)
    {   printf("file not found\n"); return;}
    freadI(fp,&version);
    fseek(fp,*loc,0);
    for(i=0; i<*nc; i++)
    {   
        freadI(fp,&node_id);
        if(version==20030722)
        { //floats read into doubles
            freadF2D(fp,&(node_coordinates[0]));//x
            freadF2D(fp,&(node_coordinates[1]));} //y
        else if(version==20030802)
        { //doubles read into doubles
            freadD(fp,&(node_coordinates[0]));//x
            freadD(fp,&(node_coordinates[1]));} //y
#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%7d %19.9f %19.9f\n",node_id,node_coordinates[0],
                node_coordinates[1]);
#endif
        n[i].setparameters(node_id, node_coordinates);}
    *loc=ftell(fp);
    fclose(fp);
#ifdef DEBUGFUNKYBIN
    fclose(fpD);
#endif
#else
    ifstream inDatafile("funky.dat", ios::in);
    if(!inDatafile)
        cout << "file not found\n";
    inDatafile.seekg(*loc);
    for(i = 0; i < *nc; i++)
    {
        inDatafile >> node_id;
        inDatafile >> node_coordinates[0];
        inDatafile >> node_coordinates[1];
        
        n[i].setparameters(node_id, node_coordinates);
        
    }
    *loc = inDatafile.tellg();
    inDatafile.close();
#endif
}

void Read_element_data(int* ec, NodePreproc n[], ElementPreproc e[], long* loc)
{
    int elem_id;
    int element_nodes[8];
    int material;
    int elm_loc[2];
    int i, j, w;
    
    NodePreproc* address[8];
#ifdef BINARYPRE
    FILE *fp=fopen_bin("funky.bin","r");
#ifdef DEBUGFUNKYBIN
    FILE *fpD=fopen("funky.bin.debug","a");
#endif
    if(!fp)
    {   printf("file not found\n"); return;}
    fseek(fp,*loc,0);

    for(i=0; i<*ec; i++)
    {   
        freadI(fp,&elem_id);
#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%8d ",elem_id);
#endif
        for(j=0; j<8; j++)
        {   
            freadI(fp,&(element_nodes[j]));
#ifdef DEBUGFUNKYBIN
            fprintf(fpD,"%8d ",element_nodes[j]);
#endif
        }

        for(j=0; j<8; j++)
        {   
            w = element_nodes[j] - 1;
            if(n[w].get_nodeid()!=element_nodes[j])
            {   
                w = 0;
                while(n[w].get_nodeid()!=element_nodes[j]) w++;
                printf("found the node the long way\n");fflush(stdout);}
            address[j]=&n[w];}

        freadI(fp,&material);
        freadI(fp,&(elm_loc[0]));
        freadI(fp,&(elm_loc[1]));
#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%8d %8d %8d\n",material,elm_loc[0],elm_loc[1]);
#endif
        e[i].setparameters(elem_id, address, material-1, elm_loc);}

    *loc=ftell(fp);
    fclose(fp);
#ifdef DEBUGFUNKYBIN
    fclose(fpD);
#endif
#else
    ifstream inDatafile("funky.dat", ios::in);
    if(!inDatafile)
        cout << "file not found\n" << flush;
    inDatafile.seekg(*loc);
    
    for(i = 0; i < *ec; i++)
    {
        inDatafile >> elem_id;
        for(j = 0; j < 8; j++)
            inDatafile >> element_nodes[j];
        
        for(j = 0; j < 8; j++)
        {
            w = element_nodes[j] - 1;
            if(n[w].get_nodeid() != element_nodes[j])
            {
                w = 0;
                while (n[w].get_nodeid() != element_nodes[j])
                    w++;
                cout << "found the node the long way\n" << flush;
            }
            address[j] = &n[w];
        }
        
        inDatafile >> material;
        inDatafile >> elm_loc[0];
        inDatafile >> elm_loc[1];
        e[i].setparameters(elem_id, address, material - 1, elm_loc);
    }
    
    *loc = inDatafile.tellg();
    inDatafile.close();
#endif
}

void Read_boundary_data(int* fc, int* cc, NodePreproc n[], BoundaryPreproc b[], long* loc)
{
    int bound_id, i, w;
    double xcomp;
    double ycomp;
    
#ifdef BINARYPRE
    FILE *fp=fopen_bin("funky.bin","r");
    int version;
#ifdef DEBUGFUNKYBIN
    FILE *fpD=fopen("funky.bin.debug","a");
#endif
    if(!fp)
    {   printf("file not found\n"); fflush(stdout); return;}
    freadI(fp,&version);
    fseek(fp,*loc,0);

    for(i=0; i<*cc; i++)
    { //first constraints, then forces
        freadI(fp,&bound_id);
        if(version==20030722)
        { //floats read into doubless
            freadF2D(fp,&xcomp);
            freadF2D(fp,&ycomp);}
        else if(version==20030802)
        { //doubles read into doubles
            freadD(fp,&xcomp);
            freadD(fp,&ycomp);}

#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%6d %7.3f %7.3f\n",bound_id,xcomp,ycomp);
#endif
        w=0;
        while(n[w].get_nodeid()!=bound_id) w++;
        b[i].setparameters(&n[w], xcomp, ycomp, -2);}

    for(i=*cc; i<(*fc+*cc); i++)
    {   
        freadI(fp,&bound_id);
        if(version==20030722)
        { //floats read into doubles
            freadF2D(fp,&xcomp);
            freadF2D(fp,&ycomp);}
        else if(version==20030802)
        { //doubles read into doubles
            freadD(fp,&xcomp);
            freadD(fp,&ycomp);}

#ifdef DEBUGFUNKYBIN
        fprintf(fpD,"%6d %7.3f %7.3f\n",bound_id,xcomp,ycomp);
#endif
        w=0;
        while(n[w].get_nodeid()!=bound_id) w++;
        b[i].setparameters(&n[w], xcomp, ycomp, -3);}

    *loc=ftell(fp);
    fclose(fp);
#ifdef DEBUGFUNKYBIN
    fclose(fpD);
#endif
#else
    ifstream inDatafile("funky.dat", ios::in);
    if(!inDatafile)
        cout << "file not found\n" << flush;
    inDatafile.seekg(*loc);
    
    for(i = 0; i < *cc; i++) //first constraints, then forces
    {
        inDatafile >> bound_id;
        inDatafile >> xcomp;
        inDatafile >> ycomp;
        w = 0;
        while (n[w].get_nodeid() != bound_id)
            w++;
        b[i].setparameters(&n[w], xcomp, ycomp, -2);
    }
    
    for(i = *cc; i < (*fc + *cc); i++)
    {
        inDatafile >> bound_id;
        inDatafile >> xcomp;
        inDatafile >> ycomp;
        w = 0;
        while (n[w].get_nodeid() != bound_id)
            w++;
        b[i].setparameters(&n[w], xcomp, ycomp, -3);
    }
    
    *loc = inDatafile.tellg();
    inDatafile.close();
#endif
}

//Read_material_data() is also called in createfunky.C... do not delete 
void TitanPreproc::Read_material_data(int *material_count, char ***materialnames, double **lambda, double **mu)
{
    
    //material_map.print0();
    //read in the material names and properties from file "frict.data"
    //FILE *fp = fopen("frict.data", "r");
    //fscanf(fp, "%d", material_count); //number of materials
    *material_count = matprops->material_count;
    
    //material id tags/indices start from 1
    *lambda = CAllocD1(*material_count + 1); //internal friction angle
    *mu = CAllocD1(*material_count + 1); //bed friction angle
    *materialnames = (char **) malloc((*material_count + 1) * sizeof(char *));
    //char tempstring[200];
    
    for(int imat = 1; imat <= *material_count; imat++)
    {
        //fgets(tempstring, 200, fp); //get rid of newline at end of previous line
        
        //fgets(tempstring, 200, fp); //read the material name
        //replace newline with null character
        //tempstring[strlen(tempstring) - 1] = '\0';
        (*materialnames)[imat] = allocstrcpy(matprops->matnames[imat ].c_str());
        
        //read in internal and bed friction angles
        (*lambda)[imat] = integrator->int_frict;
        (*mu)[imat] = matprops->bedfrict[imat];
        //fscanf(fp, "%lf", &((*lambda)[imat]));
        //fscanf(fp, "%lf", &((*mu)[imat]));
    }
    //fclose(fp);
    
    return;
}

//**************************FINDING THE NEIGHBORS******************************
void Determine_neighbors(int element_count, ElementPreproc* element, int node_count, NodePreproc* node)
{
    int i, j;
    for(i = 0; i < node_count; i++)
        node[i].put_element_array_loc(-1);
    
    for(i = 0; i < element_count; i++)
        for(j = 4; j < 8; j++)
            (*(element[i].get_element_node() + j))->put_element_array_loc(i);
    
    for(i = 0; i < element_count; i++)
        element[i].determine_neighbors(i, element);
    
    for(i = 0; i < element_count; i++)
        element[i].determine_opposite_brother();
    
    return;
}

//**************************DATA OUTPUT******************************

void Write_data(int np, int nc, int ec, int bc, int mc, NodePreproc n[], ElementPreproc* o[], BoundaryPreproc b[],
                unsigned maxk[], unsigned mink[], double min[], double max[], char **materialnames, double* lambda,
                double* mu)
{
    
    char filename[14] = "funkyxxxx.inp";
    int el_per_proc = ec / np;
    int subdomain_nodes;
    int written;
    
    int i, j, k;
    int x, c;
    
    for(i = 0; i < np; i++)
    {
        subdomain_nodes = 0;
        written = 0;
        sprintf(filename, "funky%04d.inp", i);
        
        double doublekeyrange[2];
        doublekeyrange[1] = pow(2.0, sizeof(unsigned) * 8) + 1; //max is 2^32-1 but starts at zero which makes range 2^32 and add 1 to make odd, use this for every unsigned variable in key except the zeroth (starting from zero) when have higher dimensions. 
        doublekeyrange[0] = doublekeyrange[1] / np; //this will never be a whole number because np=2^integer, we want a fractional number here.
                
#ifdef BININPUT
        FILE *fp = fopen_bin(filename, "w");
        if(!fp)
        {
            printf("Could not be created!!!\n");
            return;
        }
#ifdef WRITEDOUBLEASFLOAT
        fwriteI(fp,20061109); //version number: date file format was established: 2006 November 9
        //fwriteI(fp,20030824); //version number: date file format was established
#else
        fwriteI(fp, 20061110); //version number: date file format was established: 2006 November 10
        //fwriteI(fp,20030825); //version number: date file format was established
#endif
        
#else
        ofstream outDatafile(filename, ios::out);
        if(!outDatafile) cout<<"Could not be created!!!"<<'\n';
#endif
        
        c = i * el_per_proc;
        if(i != np - 1)
            x = (i + 1) * el_per_proc;
        else
            x = ec;
        
        if(i > 0)
            for(j = 0; j < nc; j++)
                n[j].clear_written_flag();
        
        for(j = c; j < x; j++)
            for(k = 0; k < 9; k++)
            {
                written = (*((o[j])->get_element_node() + k))->get_written_flag();
                if(written == 0)
                {
                    subdomain_nodes++;
                    (*((o[j])->get_element_node() + k))->set_written_flag();
                }
            }
        
#ifdef BININPUT
        fwriteI(fp, subdomain_nodes);
        
#ifdef WRITEDOUBLEASFLOAT
        fwriteF(fp,doublekeyrange[0]); //range of first part of key on every processor
        fwriteF(fp,doublekeyrange[1]);//range of second part of key on every processor
        fwriteF(fp,min[0]);//min x
        fwriteF(fp,max[0]);//max x
        fwriteF(fp,min[1]);//min y
        fwriteF(fp,max[1]);//max y
#else
        fwriteD(fp, doublekeyrange[0]); //range of first part of key on every processor
        fwriteD(fp, doublekeyrange[1]); //range of second part of key on every processor
        fwriteD(fp, min[0]); //min x
        fwriteD(fp, max[0]); //max x
        fwriteD(fp, min[1]); //min y
        fwriteD(fp, max[1]); //max y
#endif
        
        for(j = 0; j < nc; j++)
            n[j].clear_written_flag();
        
        for(j = c; j < x; j++)
            for(k = 0; k < 9; k++)
            {
                if(k == 8)
                    (*((o[j])->get_element_node() + k))->clear_written_flag();
                (*((o[j])->get_element_node() + k))->write_node_data_bin(fp);
            }
        
        //element data start here
        
        fwriteI(fp, x - c); //number of elements
                
        for(j = c; j < x; j++)
            (o[j])->write_element_data_bin(fp);
        
        //material properties start here
        fwriteI(fp, mc); //number of materials 
                
        char tempmatname[20];
#ifdef WRITEDOUBLEASFLOAT
        for(int imat=1;imat<=mc;imat++)
        {   
            fwritestring(fp,materialnames[imat]);
            fwriteF(fp,lambda[imat]);
            fwriteF(fp,mu[imat]);}
#else
        for(int imat = 1; imat <= mc; imat++)
        {
            fwritestring(fp, materialnames[imat]);
            fwriteD(fp, lambda[imat]);
            fwriteD(fp, mu[imat]);
        }
#endif
        
        fclose(fp);
#else
        assert(0); //text funky is outdated.
        outDatafile<<subdomain_nodes<<' '<<mink[0]<<' '<<mink[1]<<' '<<' '<<maxk[0]<<' '<<maxk[1]<<'\n';

        outDatafile<<min[0]<<" "<<max[0]<<' '<<min[1]<<' '<<max[1]<<endl;

        for(j=0; j<nc; j++)
        n[j].clear_written_flag();

        for(j=c; j<x; j++)
        for(k=0; k<9; k++)
        {   
            if(k==8)
            (*((o[j])->get_element_node()+k))->clear_written_flag();
            (*((o[j])->get_element_node()+k))->write_node_data(&outDatafile);}

        //element data start here
        
        outDatafile<<x-c<<' '<<*((*((o[c])->get_element_node()+8))->get_key())<<' '<<*((*((o[c])->get_element_node()+8))->get_key()+1)<<'\n';

        outDatafile<<*((*((o[x-1])->get_element_node()+8))->get_key())<<' '<<*((*((o[x-1])->get_element_node()+8))->get_key()+1)<<'\n';

        for(j=c; j<x; j++)
        {   
            (o[j])->write_element_data(&outDatafile);
            outDatafile<<'\n';}

        outDatafile<<mc<<endl;
        for(imat=1;imat<=mc;imat++)
        {   
            outDatafile<<materialnames[imat]<<endl;
            outDatafile<<lambda[imat]<<"  "<<mu[imat]<<endl;}

#endif    
        
    }
    
}

