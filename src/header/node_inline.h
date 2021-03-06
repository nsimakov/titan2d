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
 * $Id: node.h 233 2012-03-27 18:30:40Z dkumar $ 
 */

#ifndef NODE_INLINE_H
#define NODE_INLINE_H

inline Node::Node()
{
    //init();
}

inline Node::Node(const SFC_Key& keyi, double *coordi, MatProps *matprops_ptr)
{
    init(keyi, coordi, matprops_ptr);
}

inline Node::Node(const SFC_Key& keyi, double *coordi, int inf, int ord, MatProps *matprops_ptr)
{
    init(keyi, coordi, inf, ord, matprops_ptr);
}

inline Node::Node(const SFC_Key& keyi, double* coordi, int inf, int ord, double elev, int yada)
{
    init(keyi, coordi, inf, ord, elev, yada);
}

inline Node::Node(FILE* fp, MatProps* matprops_ptr) //for restart
{
    init(fp, matprops_ptr);
}

inline int Node::info() const {return nodeHashTable->info_[ndx_];}
inline void Node::info(int in){nodeHashTable->info_[ndx_] = in;}

inline const SFC_Key& Node::key() const
{
    return nodeHashTable->key_[ndx_];
}
inline void Node::set_key(const SFC_Key& new_key)
{
    nodeHashTable->key_[ndx_]=new_key;
}

inline double Node::coord(int idim) const {return nodeHashTable->coord_[idim][ndx_];}
inline void Node::coord(int idim, double new_crd){nodeHashTable->coord_[idim][ndx_]=new_crd;}   

inline int Node::id() const {return nodeHashTable->id_[ndx_];}
inline void Node::id(int id_in){nodeHashTable->id_[ndx_] = id_in;}


inline double Node::flux(int idim) const {return nodeHashTable->flux_[idim][ndx_];}
inline void Node::flux(int idim, double value){nodeHashTable->flux_[idim][ndx_]=value;}

inline void Node::zero_flux(){for(int i = 0; i < NUM_STATE_VARS; i++){flux(i,0.0);refinementflux(i, 0.0);}}

inline double Node::refinementflux(int idim) const {return nodeHashTable->refinementflux_[idim][ndx_];}
inline void Node::refinementflux(int idim, double value){nodeHashTable->refinementflux_[idim][ndx_]=value;}

inline double Node::elevation() const {return nodeHashTable->elevation_[ndx_];}
inline double & Node::elevation_ref() {return nodeHashTable->elevation_[ndx_];}

inline void Node::elevation(double new_elevation){nodeHashTable->elevation_[ndx_]=new_elevation;}

inline void Node::num_assoc_elem(int numin){nodeHashTable->num_assoc_elem_ [ndx_]= numin;}
inline int Node::num_assoc_elem() const {return nodeHashTable->num_assoc_elem_[ndx_];}

inline void Node::connection_id(int id){nodeHashTable->connection_id_[ndx_] = id;}
inline int Node::connection_id() const {return nodeHashTable->connection_id_[ndx_];}

inline ti_ndx_t Node::ndx() const {return ndx_;}
inline void Node::ndx(ti_ndx_t new_ndx) {ndx_ = new_ndx;}


#endif
