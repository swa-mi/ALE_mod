//all code by Szollosi GJ et al.; ssolo@elte.hu; CC BY-SA 3.0;
#include "ALE.h"
struct step 
{
  int e;
  int ep;
  int epp;
  scalar_type t;
  int rank;
  long int g_id;
  long int gp_id;
  long int gpp_id;
  std::string event;
};


/****************************************************************************
 // exODT_model class.
 // This class contains the description of a time-sliced species tree,
 // with parameters of duplication, transfer and loss, and a "population size", 
 // and an approx_posterior object. This object can compute the probability of 
 // an approx_posterior given the species tree and values for the ODTL parameters.
 // Contains lots of maps to map time slices to nodes, nodes to time slices, 
 // nodes to ages...
 // Each time slice ends at a speciation node, with a particular age.
 // Each time slice has a rank, with the most recent one with rank 0, 
 // and the older one with rank number_of_species.
 *****************************************************************************/
class exODT_model
{
 public:
  
  std::map <std::string,scalar_type> scalar_parameter;//del_loc
  std::map <std::string,std::vector <scalar_type> > vector_parameter;//del_loc
  std::map <std::string,std::string> string_parameter;//del_loc

  int signal;
  std::string signal_string;

  int alpha;
  int last_branch;
  int last_rank;
  approx_posterior * ale_pointer;                            //Pointer to an approx_posterior object on which dynamic programming is performed in p for instance.

  std::map<int,int> father;                                  //del-loc. Map between node id and id of its father.
  std::map<int,std::vector<int> > daughters;                 //del-loc. Map between node id and ids of its daughters (-1 if node is a leaf).
  std::map<int,std::string> extant_species;                  //del-loc. Map between leaf id (0 to # of leaves) and leaf name.
  std::map<int, scalar_type> branch_ts;                      //del-loc. Map between branch identified by the id of the node it ends at, and time of the time slice.
  std::map<int,int>rank_ids;                                 //del-loc. Map between rank of a time slice and the id of the node that terminates it. 
  std::map<int,int>id_ranks;                                 //del-loc. Map between node id and rank of the time slice it terminates. Time slice at leaves has rank 0.

  tree_type * S;                                             //Species tree
  bpp::Node * S_root;                                        //Root of the species tree
  std::map<bpp::Node *,int>node_ids;                         //Map between node and its id.
  std::map<int,bpp::Node *>id_nodes;                         //Dual from the above, map between node id and node.

  std::map<int,scalar_type > t_begin;                        //del-loc. Map between the id of a node, and the beginning of the branch that leads to it.
  std::map<int,scalar_type > t_end;                          //del-loc. Map between the id of a node, and the end of the time slice it defines, corresponding to the age of this node.
  
  std::map<int,std::vector<int > > time_slices;              //del-loc. Map between rank of time slice and indices of branches going through it. Terminating branch is last in the vector.
  std::map<int,std::vector<int > > branch_slices;            //del-loc. Map between a branch and all the time slices it traverses.
  std::map<int,std::vector<scalar_type > > time_slice_times; //del-loc. Map between rank of time slice and all the end times of the sub-slices inside this time slice.
	std::map<int,scalar_type > time_slice_begins;            //del-loc. Map between rank of time slice and begin time of this time slice.

  //Variables used for computing.
  std::map<int,std::map <scalar_type,scalar_type> > Ee;                       //del-loc
  std::map<int,std::map <scalar_type,scalar_type> > Ge;                       //del-loc
  std::map<long int, std::map< scalar_type, std::map<int, scalar_type> > > q; //del-loc. Map between clade id (from the approx_posterior object) and a map between the time of a subslice and a map between branch id and 
  std::map<long int, std::map< scalar_type, std::map<int, step> > > q_step;   //del-loc
  std::map <long int,std::string> gid_sps;                                    //del-loc. Map between clade id (from the approx_posterior object) and species included in that clade.
 
  std::map <std::string,scalar_type> MLRec_events;                            //del-loc  
  std::map<std::string, std::vector<scalar_type> > branch_counts;             //del-loc
  std::vector<std::string> Ttokens;                                           //del-loc
  //implemented in exODT.cpp
  void construct(std::string Sstring,scalar_type N=1e6); //Constructs an object given a species tree and population size.
  exODT_model();
  ~exODT_model()
    {  
      rank_ids.clear();
      id_ranks.clear();
      father.clear();
      for (std::map<int,std::vector<int> >::iterator it=daughters.begin();it!=daughters.end();it++)
	(*it).second.clear();
      daughters.clear();
      extant_species.clear();
      branch_ts.clear();
      rank_ids.clear();
      id_ranks.clear();
      t_begin.clear();
      t_end.clear();
      for (std::map<int,std::vector<int> >::iterator it=time_slices.begin();it!=time_slices.end();it++)
	(*it).second.clear();
      time_slices.clear();
      for (std::map<int,std::vector<int> >::iterator it=branch_slices.begin();it!=branch_slices.end();it++)
	(*it).second.clear();
      for (std::map<int,std::vector<scalar_type > >::iterator it=time_slice_times.begin();it!=time_slice_times.end();it++)
	(*it).second.clear();
      time_slice_times.clear();
      time_slice_begins.clear();
      scalar_parameter.clear();
      for (std::map <std::string,std::vector <scalar_type> >::iterator it=vector_parameter.begin();it!=vector_parameter.end();it++)//del_loc
	(*it).second.clear();
      vector_parameter.clear();
      string_parameter.clear();
      node_ids.clear();
      id_nodes.clear();
      delete S;
      for (std::map<int,std::map <scalar_type,scalar_type> >::iterator it=Ee.begin();it!=Ee.end();it++)//del_loc
	(*it).second.clear();
      Ee.clear();
      for (std::map<int,std::map <scalar_type,scalar_type> >::iterator it=Ge.begin();it!=Ge.end();it++)//del_loc
	(*it).second.clear();
      Ge.clear();
      Ee.clear();
      for (std::map<long int, std::map< scalar_type, std::map<int, scalar_type> > >::iterator it=q.begin();it!=q.end();it++)
	{
	  for ( std::map< scalar_type, std::map<int, scalar_type> >::iterator jt=(*it).second.begin();jt!=(*it).second.end();jt++)
	    (*jt).second.clear();
	  (*it).second.clear();
	}      
      q.clear();
      for (std::map<long int, std::map< scalar_type, std::map<int, step> > >::iterator it=q_step.begin();it!=q_step.end();it++)
	{
	  for ( std::map< scalar_type, std::map<int, step> >::iterator jt=(*it).second.begin();jt!=(*it).second.end();jt++)
	    (*jt).second.clear();
	  (*it).second.clear();
	}      
      q_step.clear();
      gid_sps.clear();
      MLRec_events.clear();
      for (std::map<std::string, std::vector<scalar_type> >::iterator it=branch_counts.begin();it!=branch_counts.end();it++)//del_loc
	(*it).second.clear();
      branch_counts.clear();
      Ttokens.clear();
    };

  void set_model_parameter(std::string name,std::string value);    //Sets the value of a string parameter.
  void set_model_parameter(std::string,scalar_type);               //Sets the value of a scalar parameter.
  void set_model_parameter(std::string,std::vector<scalar_type>);  //Sets the value of a vector of scalars parameter.

  //implemented in model.cpp
  scalar_type p(approx_posterior *ale);                            //Computes the probability of an approx_posterior according to the species tree and parameter values.
  void calculate_EG();
  void calculate_EGb();

  //implemented in traceback.cpp
  std::pair<std::string,scalar_type> p_MLRec(approx_posterior *ale,bool lowmem=true);
  std::pair<std::string,scalar_type> traceback();
  std::string traceback(long int g_id,scalar_type t,scalar_type rank,int e,scalar_type branch_length,std::string branch_events,std::string transfer_token="");
  void register_O(int e);
  void register_D(int e);
  void register_Tto(int e);
  void register_Tfrom(int e);
  void register_L(int e);
  void register_S(int e);
  void register_leaf(int e);
  void register_Ttoken(std::string token);
  //implemented in traceback_lowmem.cpp - not done
  std::pair<std::string,scalar_type> p_MLRec_lowmem(approx_posterior *ale);
  std::string traceback_lowmem(long int g_id,scalar_type t,scalar_type rank,int e,scalar_type branch_length,std::string branch_events,std::string transfer_token="");
  //implemented in sample.cpp
  std::string sample(bool max_rec=false);
  std::string sample(bool S_node,long int g_id,int t_i,scalar_type rank,int e,scalar_type branch_length,std::string branch_events, std::string transfer_toke="",bool max_rec=false);


  void show_counts(std::string name);
  std::string counts_string();

  void show_rates(std::string name);


 private:
  ;
  
};
