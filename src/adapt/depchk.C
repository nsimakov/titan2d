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
 * $Id: depchk.C 224 2011-12-04 20:49:23Z dkumar $ 
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "../header/hpfem.h"

#include "../header/hadapt.h"

#define NumTriggerRef 256

#include <algorithm>


void HAdapt::findTriggeredRefinements(const vector<ti_ndx_t> &primaryRefinement, vector<int> &set_for_refinement, vector<ti_ndx_t> &allRefinement)

/*---
 refined[] stores the address of ready-for-refinement element of the sub-domain
 refined_temp[] stores the address of ready-for-refinement element triggered by one element refinement
 count is counting the number of refinement of the subdomain
 j is counting the number of refinement triggered by one element refinement
 ---------------*/
{
	for(ti_ndx_t primary_ndx:primaryRefinement)
	{
		int ifg=1;

		tempList.resize(0);

		//the trigger of this round of refinement:
		tempList.push_back(primary_ndx);

		int elem_to_refine = 0;

		int ielem=0;
		while ((ielem<tempList.size()) && (elem_to_refine < NumTriggerRef))
		{ //--element is temporary varible
			ti_ndx_t ndx=tempList[ielem];

			for(int i = 0; i < 4; i++)
			{ //-- checking the four neighbors to identify which must be refined

				int neigh_proc = ElemTable->neigh_proc_[i][ndx];

				if((neigh_proc != -1) && (neigh_proc != -2))
				{ //-- if there is a neighbor

					ti_ndx_t neigh_ndx = ElemTable->lookup_ndx(ElemTable->neighbors_[i][ndx]);//ElemTable->neighbor_ndx_[i][ndx];

					//assert(Neigh);
					if(ti_ndx_not_negative(neigh_ndx) && neigh_proc == myid)
					{ //-- if this neighbor is in the same proc as element is

						if(ElemTable->generation_[ndx] > ElemTable->generation_[neigh_ndx])
						{
							//-- if the neighbor is bigger, then it must be refined

							if((ElemTable->adapted_[neigh_ndx] == NOTRECADAPTED) || (ElemTable->adapted_[neigh_ndx] == NEWFATHER))
							{
								if(find(tempList.begin(), tempList.end(), neigh_ndx)==tempList.end() )
								{ //-- if this neighbor has not yet been marked
									tempList.push_back(neigh_ndx);
									++elem_to_refine;
								}
							}
							else if(ElemTable->adapted_[neigh_ndx] != OLDFATHER)
							{
								ifg = 0;
								break;
							}
						}

					}
					else
					{ //-- need neighbor's generation infomation

						if(ElemTable->generation_[ndx] > ElemTable->generation_[neigh_ndx])
						{ //--stop this round of refinement

							ifg = 0;
							break;
						}
					}
				}

			}
			if(!ifg)
				break;
			++ielem;
			//k++;
			//element = TempList.get(k); //--check next
		}

		//copy TempList to RefinedList
		if(ifg)
		{
			if(elem_to_refine < NumTriggerRef) //-- NumTriggerRef is the maximum tolerence of related refinement
				for(int m = 0; m < tempList.size(); m++)
				{
					if(set_for_refinement[tempList[m]]==0)
					{
						allRefinement.push_back(tempList[m]);
						set_for_refinement[tempList[m]]=1;
					}
				}
			else
			{
				ifg = 0; //-- refuse to do the refinement
			}
		}
	}
	return;
}


void depchk(Element* EmTemp, ElementsHashTable* ElemTable, NodeHashTable* NodeTable, int* ifg, ElemPtrList* RefinedList)

/*---
 refined[] stores the address of ready-for-refinement element of the sub-domain
 refined_temp[] stores the address of ready-for-refinement element triggered by one element refinement
 count is counting the number of refinement of the subdomain
 j is counting the number of refinement triggered by one element refinement
 ---------------*/
{
    
    int i, j, k;
    Element* element;
    Element* Neigh;
    ElemPtrList TempList(ElemTable, 384);
    int myid;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    
    TempList.add(EmTemp);
    
    j = 0;
    k = 0;
    element = EmTemp; //-- EmTemp is the trigger of this round of refinement
            
    while (element && (j < NumTriggerRef))
    { //--element is temporary varible
    
        for(i = 0; i < 4; i++)
        { //-- checking the four neighbors to identify which must be refined
        
            int neigh_proc = element->neigh_proc(i);
            
            if((neigh_proc != -1) && (neigh_proc != -2))
            { //-- if there is a neighbor
            
                Neigh = (Element*) (ElemTable->lookup(element->neighbor(i)));
                
                //assert(Neigh);
                if(Neigh != NULL && neigh_proc == myid)
                { //-- if this neighbor is in the same proc as element is
                
                    if(element->generation() > Neigh->generation())
                    {
                        //-- if the neighbor is bigger, then it must be refined
                        
                        if((Neigh->adapted_flag() == NOTRECADAPTED) || (Neigh->adapted_flag() == NEWFATHER))
                        {
                            int flag = 1;
                            for(int m = 0; m < TempList.get_num_elem(); m++)
                                if(TempList.get_key(m)==Neigh->key())
                                {
                                    flag = 0;
                                    break;
                                }
                            
                            if(flag)
                            { //-- if this neighbor has not yet been marked
                            
                                j++;
                                TempList.add(Neigh);
                            }
                        }
                        else if(Neigh->adapted_flag() != OLDFATHER)
                        {
                            *ifg = 0;
                            TempList.trashlist();
                            break;
                        }
                    }
                    
                }
                else
                { //-- need neighbor's generation infomation
                
                    if(element->generation() > element->neigh_gen(i))
                    { //--stop this round of refinement
                    
                        *ifg = 0;
                        TempList.trashlist();
                        break;
                    }
                }
            }
            
        }
        if(!*ifg)
            break;
        k++;
        element = TempList.get(k); //--check next
    }
    
    //copy TempList to RefinedList
    if(*ifg)
    {
        
        if(j < NumTriggerRef) //-- NumTriggerRef is the maximum tolerence of related refinement
            for(int m = 0; m < TempList.get_num_elem(); m++)
            {
                int sur = 0;
                for(int mi = 0; mi < RefinedList->get_num_elem(); mi++)
                    if(sur = (RefinedList->get_key(mi)==TempList.get_key(m)))
                        break;
                if(!sur)
                    RefinedList->add(TempList.get(m));
            }
        else
        {
            
            *ifg = 0; //-- refuse to do the refinement
        }
    }
    
    for(int m = 0; m < TempList.get_num_elem() - 1; m++)
        for(int mi = m + 1; mi < TempList.get_num_elem(); mi++)
            assert(TempList.get_key(m)!=TempList.get_key(mi));
    
    return;
}
#ifdef DISABLED
void depchk(Element* EmTemp, ElementsHashTable* El_Table, int* ifg, Element* refined[], int* count)

/*---
 refined[] stores the address of ready-for-refinement element of the sub-domain
 refined_temp[] stores the address of ready-for-refinement element triggered by one element refinement
 count is counting the number of refinement of the subdomain
 j is counting the number of refinement triggered by one element refinement
 ---------------*/
{   

    int i, j, k;
    Element* element;
    Element* Neigh;
    Element* refined_temp[128];
    void* p;
    int myid, numprocs;
    unsigned send_buf[4*KEYLENGTH];
    unsigned recv_buf[4*KEYLENGTH];

    MPI_Status status;
    MPI_Request request;

    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

    for(j=1;j<128;j++) refined_temp[j] = NULL;
    refined_temp[0] = EmTemp;

    j = 0; k = 0;
    element = EmTemp; //-- EmTemp is the trigger of this round of refinement
    
    while(element&&(j<10))//--element is temporary varible
    {   
        for(i=0;i<4;i++) //-- checking the four neighbors to identify which must be refined
        {   
            int neigh_proc = *(element->get_neigh_proc()+i);

            if((neigh_proc!=-1)&&(neigh_proc!=-2)) //-- if there is a neighbor
            {   

                Neigh = (Element*)(El_Table->lookup(element->get_neighbors()+i*KEYLENGTH));

                assert(Neigh);
                if(Neigh != NULL && neigh_proc == myid) //-- if this neighbor is in the same proc as element is
                {   
                    if((!Neigh->get_refined_flag())&&element->get_gen()>Neigh->get_gen()) //-- if the neighbor is bigger, then it must be refined
                    {   
                        int flag = 1; int m = 0;
                        while(refined_temp[m])
                        {   
                            if(compare_key(refined_temp[m]->pass_key(), Neigh->pass_key()))
                            {   flag = 0; break;}
                            else m++;
                            assert(m<128);
                        }

                        if(flag) //-- if this neighbor has not yet been marked
                        {   
                            j++;
                            assert(j<128);
                            refined_temp[j] = Neigh;
                        }
                    }
                }
                else //-- need neighbor's generation infomation
                {   
                    if(element->get_gen()>*(element->get_neigh_gen()+i)) //--stop this round of refinement
                    {   
                        *ifg = 0;
                        for(int m=0;m<128;m++) refined_temp[m] = NULL;
                        break;
                    }
                }
            }

        }
        if(!*ifg) break;
        k++;
        element = refined_temp[k]; //--check next
    }

    if(*ifg)
    {   
        if(j<10) //-- 10 is the maximum tolerence of related refinement
        {   
            int m = 0;
            while(refined_temp[m])
            {   
                int sur = 0; int mi = 0;
                while(refined[mi])
                {   
                    if(compare_key(refined[mi]->pass_key(), refined_temp[m]->pass_key())) //-- KEYLENGTH should be considered
                    {   
                        sur = 1;
                        break;
                    }
                    mi++;
                }
                if(!sur)
                {   
                    refined[*count] = refined_temp[m];
                    *count = *count+1;
                }
                m++;
            }
        }
        else
        {   
            *ifg = 0; //-- refuse to do the refinement
        }
    }
}

#endif

