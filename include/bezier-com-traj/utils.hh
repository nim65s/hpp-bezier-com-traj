/*
 * Copyright 2018, LAAS-CNRS
 * Author: Steve Tonneau
 */

#ifndef BEZIER_COM_TRAJ_LIB_UTILS_H
#define BEZIER_COM_TRAJ_LIB_UTILS_H

#include <bezier-com-traj/config.hh>
#include <bezier-com-traj/definitions.hh>
#include <bezier-com-traj/flags.hh>

#include <Eigen/Dense>

#include <vector>

namespace bezier_com_traj
{

/**
 * @brief Compute the Bernstein polynoms for a given degree
 * @param degree required degree
 * @return the bernstein polynoms
 */
BEZIER_COM_TRAJ_DLLAPI std::vector<spline::Bern<double> > ComputeBersteinPolynoms(const unsigned int degree);


/**
 * @brief given the constraints of the problem, and a set of waypoints, return
 * the bezier curve corresponding
 * @param pData problem data
 * @param T total trajectory time
 * @param pis list of waypoints
 * @return the bezier curve
 */
template<typename Point>
BEZIER_COM_TRAJ_DLLAPI bezier_t computeBezierCurve(const ConstraintFlag& flag, const double T,
                                                   const std::vector<Point>& pi, const Point& x);

/**
 * @brief write a polytope describe by A x <= b linear constraints in
 * a given filename
 * @return the bernstein polynoms
 */
void printQHullFile(const std::pair<MatrixXX, VectorX>& Ab,VectorX intPoint,
                    const std::string& fileName,bool clipZ = false);

/**
 * @brief skew symmetric matrix
 */
BEZIER_COM_TRAJ_DLLAPI Matrix3 skew(point_t_tC x);

/**
 * @brief normalize inequality constraints
 */
int Normalize(Ref_matrixXX A, Ref_vectorX b);

} // end namespace bezier_com_traj

template<typename Point>
bezier_com_traj::bezier_t bezier_com_traj::computeBezierCurve(const ConstraintFlag& flag, const double T,
                                                   const std::vector<Point>& pi, const Point& x)
{
    std::vector<Point> wps;
    size_t i = 0;
    if(flag & INIT_POS ){
        wps.push_back(pi[i]);
        i++;
        if(flag & INIT_VEL){
            wps.push_back(pi[i]);
            i++;
            if(flag & INIT_ACC){
                wps.push_back(pi[i]);
                i++;
            }
        }
    }
    wps.push_back(x);
    i++;
    if(flag & END_ACC){
        assert(pData.constraints_.flag_ & END_VEL && "You cannot constrain final acceleration if final velocity is not constrained.");
        wps.push_back(pi[i]);
        i++;
    }
    if(flag & END_VEL){
        assert(pData.constraints_.flag_ & END_POS && "You cannot constrain final velocity if final position is not constrained.");
        wps.push_back(pi[i]);
        i++;
    }
    if(flag & END_POS){
        wps.push_back(pi[i]);
        i++;
    }
    return bezier_t (wps.begin(), wps.end(),T);
}


#endif
