#include "exODT.h"
using namespace std;
using namespace bpp;

exODT_model::exODT_model()
{
  //some default parameters
  string_parameter["gene_name_separators"]="_@";
  scalar_parameter["species_field"]=0;
  scalar_parameter["event_node"]=0;
  scalar_parameter["min_bip_count"]=-1;
  scalar_parameter["min_branch_lenghts"]=0;  
  // length of "stem" branch above root
  scalar_parameter["stem_length"]=1;
  //number of discretization slices per time slice
  scalar_parameter["D"]=3;
  //number of subdiscretizations for ODE calculations
  scalar_parameter["DD"]=10;
}


void exODT_model::construct(string Sstring,scalar_type N)
{
  string_parameter["S_in"]=Sstring;
  //virtual branch
  alpha=-1;
  last_branch=0;
  S=TreeTemplateTools::parenthesisToTree(string_parameter["S_in"]);//del-loc

  S_root = S->getRootNode();//del-loc
  vector <Node*> leaves = TreeTemplateTools::getLeaves(*S_root);//del-loc
  //sort leaves according to name
  map <string,Node *> leaf_sort; //map storing leaves according to their names
  for (vector <Node * >::iterator it=leaves.begin();it!=leaves.end();it++ )
    leaf_sort[(*it)->getName()]=(*it);
  leaves.clear();
  for (map <string,Node * >::iterator it=leaf_sort.begin();it!=leaf_sort.end();it++ )
    leaves.push_back((*it).second);
  leaf_sort.clear();
	//leaves is now sorted by alphabetical order of the names
  map <Node*,int> next_generation; //Map between node and slice id of descendant nodes.
  map <Node*,scalar_type> node_ts; //Map between node and its time.

  // register extant species
  for (vector <Node * >::iterator it=leaves.begin();it!=leaves.end();it++ )
    {
      Node * node = (*it);
      // a leaf
      id_ranks[last_branch]=0;
      daughters[last_branch].push_back(-1);
      // a leaf
      daughters[last_branch].push_back(-1);
      extant_species[last_branch]=node->getName();
      node_ts[node]=0;
      branch_ts[last_branch]=0;
      node_ids[node]=last_branch;
      id_nodes[last_branch]=node;
      next_generation[node->getFather()]=-1;
      last_branch++;
    }

  // make sure S is ultrametric
  while(1)
    {
      vector <Node*> tmp;
      bool stop=true;
      for (map <Node *,int >::iterator it=next_generation.begin();it!=next_generation.end();it++ )
	if (next_generation[(*it).first]==-1) //father of leaves at first, then a node that needs to be examined
	  {  
	    Node * node = (*it).first;
	    vector <Node *> sons=node->getSons();//del-loc
	    if (node_ts.count(sons[0])!=0 and node_ts.count(sons[1])!=0)
	      {
		
		scalar_type l0= sons[0]->getDistanceToFather();
		scalar_type l1= sons[1]->getDistanceToFather();
		scalar_type h0= node_ts[sons[0]];
		scalar_type h1= node_ts[sons[1]];
		scalar_type d0= l0+h0;
		scalar_type d1= l1+h1;
		scalar_type d = (d0+d1) / 2;
		
		sons[0]->setDistanceToFather(d-h0);
		sons[1]->setDistanceToFather(d-h1);
		next_generation[node]=1;
		node_ts[node]=d;
		if (node->hasFather())
		  {
		    next_generation[node->getFather()]=-1;
		    stop=false;
		}
	      }
	    sons.clear();
	  }
      if (stop)
	break;     
    }

  // and has height one
  scalar_type h=node_ts[S_root];
  for (map <Node *,scalar_type >::iterator it=node_ts.begin();it!=node_ts.end();it++ )
    {
      (*it).second/=h;
      if ((*it).first->hasFather())
	{
	  scalar_type l=(*it).first->getDistanceToFather();
	  (*it).first->setDistanceToFather(l/h);
	}
    }

  string_parameter["S"] = TreeTemplateTools::treeToParenthesis(*S);
  
  //determine time order and time slices
  map <scalar_type,Node *> t_nodes;
  for (map <Node *,scalar_type >::iterator it=node_ts.begin();it!=node_ts.end();it++ )
    {
      scalar_type t=(*it).second;
      Node * node=(*it).first;
      //nonleaves
      if (t>0)
	{
	  //degenerate speciation times, where >1 nodes have same age .. should be avoided!
	  while (t_nodes.count(t)!=0 )
	    t+=1e-5;
	  t_nodes[t]=node;   
	}
    }
  for (map <scalar_type,Node * >::iterator it=t_nodes.begin();it!=t_nodes.end();it++ )
    {//we update node_ts
      scalar_type t=(*it).first;
      Node * node=(*it).second;
      node_ts[node]=t;   
    }

  last_rank=1; //the rank of the slice above the leaves
  for (map <scalar_type,Node *>::iterator it=t_nodes.begin();it!=t_nodes.end();it++ )
    {//We go through the nodes, ordered according to their age.
      scalar_type t=(*it).first;
      Node * node=(*it).second;
      branch_ts[last_branch]=t; 
	  id_ranks[last_branch]=last_rank;
      rank_ids[last_rank]=last_branch;
      node_ids[node]=last_branch;
      id_nodes[last_branch]=node;
      vector <Node *> sons=node->getSons();//del-loc
      daughters[last_branch].push_back(node_ids[sons[0]]);
      daughters[last_branch].push_back(node_ids[sons[1]]);
      father[node_ids[sons[0]]]=last_branch;
      father[node_ids[sons[1]]]=last_branch;      
      
      t_end[last_branch]=t;
      if (node->hasFather())
	t_begin[last_branch]=node_ts[node->getFather()];
      //the root
      else
	t_begin[last_branch]=t_end[last_branch]+scalar_parameter["stem_length"];
      last_rank++;
      last_branch++;
      sons.clear();      
    }
  //set t_begin for terminal branches 
  for (map <int,string>::iterator it=extant_species.begin();it!=extant_species.end();it++ )
    {
      int branch = (*it).first;
      Node * node=id_nodes[branch];
      t_begin[branch]=node_ts[node->getFather()];
    }

  for (int rank=0;rank<last_rank;rank++)
    {
      //terminal time slice terminated by present 
      if (rank==0)
	{
	  for (int branch=0;branch<last_branch;branch++)
	    if (t_end[branch]==0)
	      {
		time_slices[rank].push_back(branch);
		branch_slices[branch].push_back(rank);	  
	      }
	}
      else
	{
	  //time slice terminated by next speciation 	 
	  int terminating_branch = rank_ids[rank];
	  for(vector <int> ::iterator it=time_slices[rank-1].begin();it!=time_slices[rank-1].end();it++)
	    {
	      int branch = (*it);
	      if (father[branch]!=terminating_branch)
		{
		  time_slices[rank].push_back(branch);	  
		  branch_slices[branch].push_back(rank);	  
		}
	    }
	  //terminating branch is last in time_slices  
	  time_slices[rank].push_back(terminating_branch);
	  branch_slices[terminating_branch].push_back(rank);	  	  
	}
    }

  for (int rank=0;rank<last_rank;rank++)
    {
      scalar_type slice_end;
      int terminating_branch;
      if (rank+1<last_rank)	
	{
	  terminating_branch = rank_ids[rank];
	  slice_end=t_end[terminating_branch];
	}
      else if (rank==0)
	//rank 0 arrives at present
	slice_end=0;
      else
	//root is at t=1
	slice_end=1;
      scalar_type slice_begin;
      if (rank+1<last_rank)	
	slice_begin=t_end[rank_ids[rank+1]];
      else
	//stem above root ends itself
	slice_begin=t_begin[rank_ids[rank]];     

      scalar_type slice_height=slice_begin-slice_end;

      time_slice_times[rank].push_back(slice_end);
      for (scalar_type internal_interval=1;internal_interval<scalar_parameter["D"];internal_interval++)
	{
	  time_slice_times[rank].push_back(slice_end+internal_interval*slice_height/scalar_parameter["D"]);
	}
      time_slice_begins[rank]=slice_begin;
    }

  //annotate time orders in bootstrap values
  for (map <Node *,int >::iterator it=node_ids.begin();it!=node_ids.end();it++ )
    {
      Node * node = (*it).first;     
      int branch = (*it).second;
      stringstream out;
      stringstream out1;
      stringstream out2;
      out1<<t_begin[branch];
      out2<< t_end[branch];
      int rank=id_ranks[branch];
      out<<rank;
      node->setBranchProperty("ID",BppString(out.str()));
    }  
  string_parameter["S_with_ranks"]=TreeTemplateTools::treeToParenthesis(*S,false,"ID");
  for (map <Node *,int >::iterator it=node_ids.begin();it!=node_ids.end();it++ )
    (*it).first->setBranchProperty("ID",BppString(""));

  //event node approximation
  //cf. section S1.4 of the Supporting Material of www.pnas.org/cgi/doi/10.1073/pnas.1202997109
  //compatible with p(ale)
  //not compatible with sample() and p_MLRec(ale)
  set_model_parameter("event_node",0);
  //the calculation of depends only very weakly on the value of N  
  //default value of N=1e6 is set in exODT.h 
  set_model_parameter("N",N);
  //if we assume the that height of the species tree is equal to its expected value under the colaescent
  // cf. http://arxiv.org/abs/1211.4606
  //Delta is sigma, i.e. the speciation rate of the Moran model in http://arxiv.org/abs/1211.4606 and ALEPAPER
  set_model_parameter("Delta_bar",N*2.);
  //Lambda is not used
  set_model_parameter("Lambda_bar",N*2.);
  // delta is the gene duplication rate
  set_model_parameter("delta",0.2);
  // tau is the gene transfer rate
  set_model_parameter("tau",0.17);
  // lambda is the gene loss rate
  set_model_parameter("lambda",1.0);

  for (int branch=0;branch<last_branch;branch++)	
    {
      branch_counts["Os"].push_back(0);
      branch_counts["Ds"].push_back(0);
      branch_counts["Ts"].push_back(0);
      branch_counts["Tfroms"].push_back(0);
      branch_counts["Ls"].push_back(0);
      branch_counts["count"].push_back(0);    
      branch_counts["copies"].push_back(0);
    }
  
  //del-locs
  node_ts.clear();  
  next_generation.clear();
  leaves.clear();
}


void exODT_model::set_model_parameter(string name,string value)
{
  string_parameter[name]=value;
}


void exODT_model::set_model_parameter(string name,scalar_type value)
{

  if (name=="delta" or name=="tau" or name=="lambda")
    {      
      scalar_type N=vector_parameter["N"][0];
      vector_parameter[name].clear();
      for (int branch=0;branch<last_branch;branch++)	
	if (name=="tau")
	  vector_parameter[name].push_back(value/N);
	else
	  vector_parameter[name].push_back(value);
      if (name=="tau")
	scalar_parameter[name+"_avg"]=value/N;
      else
	scalar_parameter[name+"_avg"]=value;

    }
  else if (name=="N" or name=="Delta_bar" or name=="Lambda_bar" )
    {
      vector_parameter[name].clear();
      for (int rank=0;rank<last_rank;rank++)	
	vector_parameter[name].push_back(value);
    }
  else
    scalar_parameter[name]=value;
}


void exODT_model::set_model_parameter(string name,vector<scalar_type> value_vector)
{
  if (name=="delta" or name=="tau" or name=="lambda")
    {      
      scalar_type N=vector_parameter["N"][0];
      vector_parameter[name].clear();
      scalar_type avg=0;
      scalar_type c=0;
      for (int branch=0;branch<last_branch;branch++)	
	{
	if (name=="tau")
	  {
	    vector_parameter[name].push_back(value_vector[branch]/N);
	    avg+=value_vector[branch]/N;
	  }
	else
	  {
	    vector_parameter[name].push_back(value_vector[branch]);
	    avg+=value_vector[branch];
	  }
	  c+=1;
	}
      scalar_parameter[name+"_avg"]=avg/c;
    }
  else if (name=="N" or name=="Delta_bar" or name=="Lambda_bar" )
    {
      vector_parameter[name].clear();
      for (int rank=0;rank<last_rank;rank++)	
	vector_parameter[name].push_back(value_vector[rank]);
    }
}

