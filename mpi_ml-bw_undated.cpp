#include "ALE_util.h"
#include "mpi_tree.h"

#include <Bpp/Phyl/App/PhylogeneticsApplicationTools.h>
#include <Bpp/Phyl/OptimizationTools.h>
#include <Bpp/Numeric/AutoParameter.h>
#include <Bpp/Numeric/Function/DownhillSimplexMethod.h>

using namespace std;
using namespace bpp;
using namespace boost::mpi;

class p_fun:
  public virtual Function,
  public AbstractParametrizable
{
private:
  double fval_;
  mpi_tree* model_pointer;
  int last_branch;
public:
  p_fun(mpi_tree* model, double delta_start=0.01,double tau_start=0.01,double lambda_start=0.01,int last_branch_in) : AbstractParametrizable(""), fval_(0), model_pointer(model) 
  {
    last_branch=last_branch_in;
    //We declare parameters here:
    //   IncludingInterval* constraint = new IncludingInterval(1e-6, 10-1e-6);
    IntervalConstraint* constraint = new IntervalConstraint ( 1e-6, 10-1e-6, true, true );
    for (e=0;e<last_branch;e++)
	{
	  stringstream deltae;
	  deltae<<"delta"<<"_"<<e;
	  addParameter_( new Parameter(deltae.str(), delta_start, constraint) ) ;
	  stringstream taue;
	  taue<<"tau"<<"_"<<e;	  
	  addParameter_( new Parameter(taue.str(), tau_start, constraint) ) ;
	  stringstream lambdae;
	  lambdae<<"lambda"<<"_"<<e;
	  addParameter_( new Parameter(lambdae.str(), lambda_start, constraint) ) ;
	}

  }
  
  p_fun* clone() const { return new p_fun(*this); }
  
public:
  
    void setParameters(const ParameterList& pl)
    throw (ParameterNotFoundException, ConstraintException, Exception)
    {
        matchParametersValues(pl);
    }
    double getValue() const throw (Exception) { return fval_; }
    void fireParameterChanged(const ParameterList& pl)
    {
      vector<float> delta;
      vector<float> tau;
      vector<float> lambda;
      for (e=0;e<last_branch;e++)
	{
	  stringstream deltae;
	  deltae<<"delta"<<"_"<<e;
	  stringstream taue;
	  taue<<"tau"<<"_"<<e;	  
	  stringstream lambdae;
	  lambdae<<"lambda"<<"_"<<e;
	  double deltae = getParameterValue(deltae.str());
	  double taue = getParameterValue(taue.str());
	  double lambdae = getParameterValue(lambdae.str());
	  delta.push_back(deltae);
	  tau.push_back(taue);
	  lambda.push_back(lambdae);	  
	}
        //double sigma = getParameterValue("sigma");
        
        model_pointer->model->set_model_parameter("delta",delta);
        model_pointer->model->set_model_parameter("tau",tau);
        model_pointer->model->set_model_parameter("lambda",lambda);

        double y=-(model_pointer->calculate_pun());
        //if (world.rank()==0) cout <<endl<< "delta=" << delta << "\t tau=" << tau << "\t lambda=" << lambda << "\t ll=" << -y <<endl;
        fval_ = y;
    }
};


int main(int argc, char ** argv)
{

  environment env(argc, argv);
  communicator world;
  int done=1;

  ifstream file_stream_S (argv[1]);
  string Sstring;
  
  getline (file_stream_S,Sstring);
  map<string,scalar_type> parameters;
  mpi_tree * infer_tree = new mpi_tree(Sstring,world,parameters,true);
  infer_tree->load_distributed_ales(argv[2]);
  scalar_type ll = infer_tree->calculate_pun(10);

  
  if (world.rank()==0) cout << infer_tree->model->string_parameter["S_with_ranks"] << endl;
  infer_tree->gather_counts();
  infer_tree->gather_T_to_from();
  broadcast(world,done,0);

  
  Function* f = new p_fun(infer_tree);
  Optimizer* optimizer = new DownhillSimplexMethod(f);

  optimizer->setProfiler(0);
  optimizer->setMessageHandler(0);
  optimizer->setVerbose(0);
  if (world.rank()==0)   optimizer->setVerbose(0);


  optimizer->setConstraintPolicy(AutoParameter::CONSTRAINTS_AUTO);
  optimizer->init(f->getParameters()); //Here we optimize all parameters, and start with the default values.

    
    
 //   FunctionStopCondition stop(optimizer, 1);//1e-1);
 // optimizer->setStopCondition(stop);
    //TEMP
  //optimizer->setMaximumNumberOfEvaluations( 10 );
    
  optimizer->optimize();

  //optimizer->getParameters().printParameters(cout);
  broadcast(world,done,0);

  if (world.rank()==0)
    {
      optimizer->getParameters().printParameters(cout);
      scalar_type delta=optimizer->getParameterValue("delta");
      scalar_type tau=optimizer->getParameterValue("tau");
      scalar_type lambda=optimizer->getParameterValue("lambda");
      //scalar_type sigma=optimizer->getParameterValue("sigma");

      cout <<endl<< delta << " " << tau << " " << lambda// << " " << sigma
	   << endl;
    }
  infer_tree->gather_counts();
  infer_tree->gather_T_to_from();

}
