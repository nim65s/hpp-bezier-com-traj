/*
 * Copyright 2017, LAAS-CNRS
 * Author: Pierre Fernbach
 */

#include <bezier-com-traj/solve.hh>
#include <bezier-com-traj/common_solve_methods.hh>
#include <limits>
#include <bezier-com-traj/waypoints/waypoints_definition.hh>

using namespace bezier_com_traj;

namespace bezier_com_traj
{
typedef waypoint3_t waypoint_t;
typedef std::pair<double,point3_t> coefs_t;
const int DIM_POINT=3;
const int NUM_DISCRETIZATION = 11;
const bool verbose = false;

/**
* @brief solveEndEffector Tries to produce a trajectory represented as a bezier curve
* that satisfy position, velocity and acceleration constraint for the initial and final point
* and that follow as close as possible the input trajectory
* @param pData problem Data.
* @param path the path to follow, the class Path must implement the operator (double t) , t \in [0,1] return a Vector3
* that give the position on the path for a given time
* @param T time lenght of the trajectory
* @param timeStep time that the solver has to stop
* @return ResultData a struct containing the resulting trajectory, if success is true.
*/
template<typename Path>
ResultDataCOMTraj solveEndEffector(const ProblemData& pData,const Path& path, const double T, const double weightDistance, bool useVelCost = true);


coefs_t initCoefs(){
    coefs_t c;
    c.first=0;
    c.second=point3_t::Zero();
    return c;
}



// up to jerk second derivativ constraints for init, pos vel and acc constraint for goal
std::vector<bezier_t::point_t> computeConstantWaypointsInitPredef(const ProblemData& pData,double T){
    const double n = 4;
    std::vector<bezier_t::point_t> pts;
    pts.push_back(pData.c0_); // c0
    pts.push_back((pData.dc0_ * T / n )+  pData.c0_); //dc0
    pts.push_back((n*n*pData.c0_ - n*pData.c0_ + 2*n*pData.dc0_*T - 2*pData.dc0_*T + pData.ddc0_*T*T)/(n*(n - 1)));//ddc0 // * T because derivation make a T appear
    pts.push_back((n*n*pData.c0_ - n*pData.c0_ + 3*n*pData.dc0_*T - 3*pData.dc0_*T + 3*pData.ddc0_*T*T)/(n*(n - 1))); //j0
  //  pts.push_back((n*n*pData.c0_ - n*pData.c0_ + 4*n*pData.dc0_*T - 4*pData.dc0_ *T+ 6*pData.ddc0_*T*T)/(n*(n - 1))) ; //dj0
 //   pts.push_back((n*n*pData.c0_ - n*pData.c0_ + 5*n*pData.dc0_*T - 5*pData.dc0_ *T+ 10*pData.ddc0_*T*T)/(n*(n - 1))) ; //ddj0

   // pts.push_back((n*n*pData.c1_ - n*pData.c1_ - 2*n*pData.dc1_*T + 2*pData.dc1_*T + pData.ddc1_*T*T)/(n*(n - 1))) ; //ddc1 // * T ??
   // pts.push_back((-pData.dc1_ * T / n) + pData.c1_); // dc1
    pts.push_back(pData.c1_); //c1
    return pts;
}


// up to jerk second derivativ constraints for goal, pos vel and acc constraint for init
std::vector<bezier_t::point_t> computeConstantWaypointsGoalPredef(const ProblemData& pData,double T){
    const double n = 4;
    std::vector<bezier_t::point_t> pts;
    pts.push_back(pData.c0_); //c0
   // pts.push_back((pData.dc0_ * T / n )+  pData.c0_); //dc0
   // pts.push_back((n*n*pData.c0_ - n*pData.c0_ + 2*n*pData.dc0_*T - 2*pData.dc0_*T + pData.ddc0_*T*T)/(n*(n - 1))); //ddc0 // * T because derivation make a T appear

  //  pts.push_back((n*n*pData.c1_ - n*pData.c1_ - 5*n*pData.dc1_*T + 5*pData.dc1_*T + 10*pData.ddc1_*T*T)/(n*(n - 1))) ; //ddj1
  //  pts.push_back((n*n*pData.c1_ - n*pData.c1_ - 4*n*pData.dc1_*T + 4*pData.dc1_*T + 6*pData.ddc1_*T*T)/(n*(n - 1))) ; //dj1
    pts.push_back((n*n*pData.c1_ - n*pData.c1_ - 3*n*pData.dc1_*T + 3*pData.dc1_*T + 3*pData.ddc1_*T*T)/(n*(n - 1))) ; // j1
    pts.push_back((n*n*pData.c1_ - n*pData.c1_ - 2*n*pData.dc1_*T + 2*pData.dc1_*T + pData.ddc1_*T*T)/(n*(n - 1))) ; //ddc1 * T ??
    pts.push_back((-pData.dc1_ * T / n) + pData.c1_); // dc1
    pts.push_back(pData.c1_); //c1
    return pts;
}


void computeConstraintsMatrix(const ProblemData& pData,const std::vector<waypoint_t>& wps_acc,const std::vector<waypoint_t>& wps_vel,const VectorX& acc_bounds,const VectorX& vel_bounds,MatrixXX& A,VectorX& b,const std::vector<waypoint_t>& wps_jerk = std::vector<waypoint_t>(),const VectorX& jerk_bounds=VectorX(DIM_POINT)  ){
    assert(acc_bounds.size() == DIM_POINT && "Acceleration bounds should have the same dimension as the points");
    assert(vel_bounds.size() == DIM_POINT && "Velocity bounds should have the same dimension as the points");
    assert(jerk_bounds.size() == DIM_POINT && "Jerk bounds should have the same dimension as the points");

    int empty_acc=0;
    int empty_vel=0;
    int empty_jerk=0;
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_acc.begin(); wpcit != wps_acc.end(); ++wpcit)
    {
        if(wpcit->first.isZero(std::numeric_limits<double>::epsilon()))
            empty_acc++;
    }
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_vel.begin(); wpcit != wps_vel.end(); ++wpcit)
    {
        if(wpcit->first.isZero(std::numeric_limits<double>::epsilon()))
            empty_vel++;
    }
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_jerk.begin(); wpcit != wps_jerk.end(); ++wpcit)
    {
        if(wpcit->first.isZero(std::numeric_limits<double>::epsilon()))
            empty_jerk++;
    }

    A = MatrixXX::Zero(2*DIM_POINT*(wps_acc.size()-empty_acc+wps_vel.size()-empty_vel+wps_jerk.size() - empty_jerk)+DIM_POINT,DIM_POINT); // *2 because we have to put the lower and upper bound for each one, +DIM_POINT for the constraint on x[z]
    b = VectorX::Zero(A.rows());
    int i = 0;
    //upper acc bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_acc.begin(); wpcit != wps_acc.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = acc_bounds - wpcit->second;
            ++i;
        }
    }
    //lower acc bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_acc.begin(); wpcit != wps_acc.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = -wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = acc_bounds + wpcit->second;
            ++i;
        }
    }
    //upper velocity bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_vel.begin(); wpcit != wps_vel.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = vel_bounds - wpcit->second;
            ++i;
        }
    }
    //lower velocity bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_vel.begin(); wpcit != wps_vel.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = -wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = vel_bounds + wpcit->second;
            ++i;
        }
    }

    //upper jerk bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_vel.begin(); wpcit != wps_vel.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = vel_bounds - wpcit->second;
            ++i;
        }
    }
    //lower jerk bounds
    for (std::vector<waypoint_t>::const_iterator wpcit = wps_jerk.begin(); wpcit != wps_jerk.end(); ++wpcit)
    {
        if(! wpcit->first.isZero(std::numeric_limits<double>::epsilon())){
            A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = -wpcit->first;
            b.segment<DIM_POINT>(i*DIM_POINT)   = jerk_bounds + wpcit->second;
            ++i;
        }
    }

    // test : constraint x[z] to be always higher than init[z] and goal[z].
    // TODO replace z with the direction of the contact normal ... need to change the API
    MatrixXX mxz = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    mxz(DIM_POINT-1,DIM_POINT-1) = -1;
    VectorX nxz = VectorX::Zero(DIM_POINT);
    nxz[2]= - std::min(pData.c0_[2],pData.c1_[2]);
    A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = mxz;
    b.segment<DIM_POINT>(i*DIM_POINT)   = nxz;


    //TEST :
  /*  A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = Matrix3::Identity();
    b.segment<DIM_POINT>(i*DIM_POINT)   = Vector3(10,10,10);
    i++;
    A.block<DIM_POINT,DIM_POINT>(i*DIM_POINT,0) = -Matrix3::Identity();
    b.segment<DIM_POINT>(i*DIM_POINT)   =  Vector3(10,10,10);*/
}


template <typename Path>
void computeDistanceCostFunction(int numPoints,const ProblemData& pData, double T,const Path& path, MatrixXX& H,VectorX& g){
    H = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    g  = VectorX::Zero(DIM_POINT);
    double step = 1./(numPoints-1);
    std::vector<point_t> pi = computeConstantWaypoints(pData,T);
    std::vector<coefs_t> cks;
    for(size_t i = 0 ; i < numPoints ; ++i){
        cks.push_back(evaluateCurveAtTime(pData,pi,i*step));
    }
    point3_t pk;
    size_t i = 0;
    for (std::vector<coefs_t>::const_iterator ckcit = cks.begin(); ckcit != cks.end(); ++ckcit){
        pk=path(i*step);
      //  std::cout<<"pk = "<<pk.transpose()<<std::endl;
      //  std::cout<<"coef First : "<<ckcit->first<<std::endl;
      //  std::cout<<"coef second : "<<ckcit->second.transpose()<<std::endl;
        H += (ckcit->first * ckcit->first * Matrix3::Identity());
        g += (ckcit->first * ckcit->second) - (pk * ckcit->first);
        i++;
    }
    double norm=H(0,0); // because H is always diagonal.
    H /= norm;
    g /= norm;
}

void computeC_of_T (const ProblemData& pData,double T, ResultDataCOMTraj& res){
    std::vector<Vector3> wps = computeConstantWaypoints(pData,T);
    wps[4] = res.x; //FIXME : compute id from constraints
    res.c_of_t_ = bezier_t (wps.begin(), wps.end(),T);
    if(verbose)
      std::cout<<"bezier curve created, size = "<<res.c_of_t_.size_<<std::endl;
}

void computeVelCostFunction(int numPoints,const ProblemData& pData,double T, MatrixXX& H,VectorX& g){
    double step = 1./(numPoints-1);
    H = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    g  = VectorX::Zero(DIM_POINT);
    std::vector<coefs_t> cks;
    std::vector<point_t> pi = computeConstantWaypoints(pData,T);
    for(int i = 0 ; i < numPoints ; ++i){
        cks.push_back(evaluateVelocityCurveAtTime(pData,pi,T,i*step));
    }
    for (std::vector<coefs_t>::const_iterator ckcit = cks.begin(); ckcit != cks.end(); ++ckcit){
        H+=(ckcit->first * ckcit->first * Matrix3::Identity());
        g+=ckcit->first*ckcit->second;
    }
    //TEST : don't consider z axis for minimum acceleration cost
    //H(2,2) = 1e-6;
    //g[2] = 1e-6 ;
    //normalize :
    double norm=H(0,0); // because H is always diagonal
    H /= norm;
    g /= norm;
}

void computeAccelerationCostFunction(int numPoints,const ProblemData& pData,double T, MatrixXX& H,VectorX& g){
    double step = 1./(numPoints-1);
    H = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    g  = VectorX::Zero(DIM_POINT);
    std::vector<coefs_t> cks;
    std::vector<point_t> pi = computeConstantWaypoints(pData,T);
    for(int i = 0 ; i < numPoints ; ++i){
        cks.push_back(evaluateAccelerationCurveAtTime(pData,pi,T,i*step));
    }
    for (std::vector<coefs_t>::const_iterator ckcit = cks.begin(); ckcit != cks.end(); ++ckcit){
        H+=(ckcit->first * ckcit->first * Matrix3::Identity());
        g+=ckcit->first*ckcit->second;
    }
    //TEST : don't consider z axis for minimum acceleration cost
    //H(2,2) = 1e-6;
    //g[2] = 1e-6 ;
    //normalize :
    double norm=H(0,0); // because H is always diagonal
    H /= norm;
    g /= norm;
}

void computeJerkCostFunction(int numPoints,const ProblemData& pData,double T, MatrixXX& H,VectorX& g){
    double step = 1./(numPoints-1);
    H = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    g  = VectorX::Zero(DIM_POINT);
    std::vector<coefs_t> cks;
    std::vector<point_t> pi = computeConstantWaypoints(pData,T);
    for(int i = 0 ; i < numPoints ; ++i){
        cks.push_back(evaluateJerkCurveAtTime(pData,pi,T,i*step));
    }
    for (std::vector<coefs_t>::const_iterator ckcit = cks.begin(); ckcit != cks.end(); ++ckcit){
        H+=(ckcit->first * ckcit->first * Matrix3::Identity());
        g+=ckcit->first*ckcit->second;
    }
    //TEST : don't consider z axis for minimum acceleration cost
    //H(2,2) = 1e-6;
    //g[2] = 1e-6 ;
    //normalize :
    double norm=H(0,0); // because H is always diagonal
    H /= norm;
    g /= norm;
}


template <typename Path>
ResultDataCOMTraj solveEndEffector(const ProblemData& pData,const Path& path, const double T, const double weightDistance, bool useVelCost){


    if(verbose)
      std::cout<<"solve end effector, T = "<<T<<std::endl;
    assert (weightDistance>=0. && weightDistance<=1. && "WeightDistance must be between 0 and 1");
    double weightSmooth = 1. - weightDistance;
    std::vector<bezier_t::point_t> pi = computeConstantWaypoints(pData,T);
    std::vector<waypoint_t> wps_jerk=computeJerkWaypoints(pData,T,pi);
    std::vector<waypoint_t> wps_acc=computeAccelerationWaypoints(pData,T,pi);
    std::vector<waypoint_t> wps_vel=computeVelocityWaypoints(pData,T,pi);
    // stack the constraint for each waypoint :
    MatrixXX A;
    VectorX b;
    Vector3 jerk_bounds(10000,10000,10000);
    Vector3 acc_bounds(500,500,500);
    Vector3 vel_bounds(500,500,500);
    computeConstraintsMatrix(pData,wps_acc,wps_vel,acc_bounds,vel_bounds,A,b,wps_jerk,jerk_bounds);
  //  std::cout<<"End eff A = "<<std::endl<<A<<std::endl;
 //   std::cout<<"End eff b = "<<std::endl<<b<<std::endl;
    // compute cost function (discrete integral under the curve defined by 'path')
    MatrixXX H_rrt=MatrixXX::Zero(DIM_POINT,DIM_POINT),H_acc,H_jerk,H_smooth,H;
    VectorX g_rrt=VectorX::Zero(DIM_POINT),g_acc,g_jerk,g_smooth,g;
    if(weightDistance>0)
        computeDistanceCostFunction<Path>(50,pData,T,path,H_rrt,g_rrt);
    if(useVelCost)
        computeVelCostFunction(50,pData,T,H_smooth,g_smooth);
    else
        computeJerkCostFunction(50,pData,T,H_smooth,g_smooth);
  /*  std::cout<<"End eff H_rrt = "<<std::endl<<H_rrt<<std::endl;
    std::cout<<"End eff g_rrt = "<<std::endl<<g_rrt<<std::endl;
    std::cout<<"End eff H_acc = "<<std::endl<<H_acc<<std::endl;
    std::cout<<"End eff g_acc = "<<std::endl<<g_acc<<std::endl;
*/

    // add the costs :
    H = MatrixXX::Zero(DIM_POINT,DIM_POINT);
    g  = VectorX::Zero(DIM_POINT);
    H = weightSmooth*(H_smooth) + weightDistance*H_rrt;
    g = weightSmooth*(g_smooth) + weightDistance*g_rrt;
    if(verbose){
      std::cout<<"End eff H = "<<std::endl<<H<<std::endl;
      std::cout<<"End eff g = "<<std::endl<<g<<std::endl;
    }
    // call the solver
    VectorX init = VectorX(DIM_POINT);
    init = (pData.c0_ + pData.c1_)/2.;
   // init =pData.c0_;
    if(verbose)
      std::cout<<"Init = "<<std::endl<<init.transpose()<<std::endl;
    ResultData resQp = solve(A,b,H,g, init);

    ResultDataCOMTraj res;
    if(resQp.success_)
    {
        res.success_ = true;
        res.x = resQp.x;
       // computeRealCost(pData, res);
        computeC_of_T (pData,T,res);
       // computedL_of_T(pData,Ts,res);
    }
   if(verbose){
     std::cout<<"Solved, success = "<<res.success_<<" x = "<<res.x.transpose()<<std::endl;
     std::cout<<"Final cost : "<<resQp.cost_<<std::endl;
    }
   return res;
}


}//namespace bezier_com_traj
