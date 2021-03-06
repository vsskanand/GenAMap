//
// Created by Liuyu Jin on Jun 9, 2016.
//

#include "ICLasso.hpp"
#include <Eigen/Dense>
#include <boost/math/distributions.hpp>
#include <math.h>
#include <unordered_map>
#include <iostream>

#ifdef BAZEL
#include "model/ModelOptions.hpp"
#else
#include "ModelOptions.hpp"
#endif

using namespace Eigen;
using namespace std;


ICLasso::ICLasso() {
    lambda1 = 1;
    lambda2 = 1;
    gamma = 1;
};

void ICLasso::set_X(MatrixXf new_X){
    X = new_X;
};

void ICLasso::set_Y(MatrixXf new_Y){
    Y = new_Y;
};

void ICLasso::set_XY(MatrixXf new_X, MatrixXf new_Y){
    X = new_X;
    Y = new_Y;
    //initialize Beta here
    Beta = MatrixXf::Random(X.cols(),Y.rows());
};

void ICLasso::set_lambda1(float new_l){
    lambda1 = new_l;
};

void ICLasso::set_lambda2(float new_l){
    lambda2 = new_l;
};

void ICLasso::set_gamma(float new_g){
    gamma = new_g;
};

void ICLasso::set_theta(MatrixXf new_t){
    Theta = new_t;
};

/* helpers for cost */
float square(float a){
    return a*a;
};

MatrixXf cov(MatrixXf X){
  MatrixXf centered = X.rowwise() - X.colwise().mean();
  MatrixXf result = (centered.adjoint() * centered) / float(X.rows() - 1);
  return result;
};

float sign(float x){
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
};

/* end helpers for cost */

float ICLasso::cost() {
    int n = X.cols();
    MatrixXf YXBeta = Y-X*Beta;
    MatrixXf squared = YXBeta.unaryExpr(std::ptr_fun(square));
    float loss1 = squared.sum()/n;
    float loss2 = (cov(Y)*Theta).trace() - log(Theta.determinant());
    float pen1 = Beta.cwiseAbs().sum();
    float pen2 = Theta.cwiseAbs().sum();
    float pen3 = 0;
    float incr;
    int size_T = Theta.rows();
    for (int i = 0; i < size_T; i++){
        for (int j = i+1; j < size_T; j++){
            incr = ((Beta.col(i) + sign(Theta(i,j))*Beta.col(j)).cwiseAbs().sum());
            pen3 = pen3 + abs(Theta(i, j))*incr;
        }
    }
    return loss1 + loss2 + lambda1 * pen1 + lambda2 * pen2 + gamma * pen3;
};

/* Helpers for optimize_theta */
float cost_theta(MatrixXf S, MatrixXf Beta, MatrixXf Theta, float lambda, float gamma) {
    float loss = (S*Theta).trace()-log(Theta.determinant());
    float pen2 = Theta.cwiseAbs().sum();
    float pen3 = 0;
    int size_T = Theta.rows();
    float incr;
    for (int i = 0; i < size_T; i++){
        for (int j = i+1; j < size_T; j++){
            incr = ((Beta.col(i) + sign(Theta(i,j))*Beta.col(j)).cwiseAbs().sum());
            pen3 = pen3 + abs(Theta(i, j))*incr;
        }
    }
    return loss + lambda * pen2 + gamma * pen3;
};


MatrixXf remove_row(MatrixXf X, int j){
    int numRows = X.rows()-1;
    int numCols = X.cols();
    MatrixXf result = MatrixXf::Zero(numRows, numCols);
    result.block(0, 0, j, numCols) = X.block(0, 0, j, numCols);
    result.block(j, 0, numRows-j, numCols) = X.block(j+1, 0, numRows-j, numCols);
    return result;
};

MatrixXf remove_col(MatrixXf X, int j){
    int numRows = X.rows();
    int numCols = X.cols() - 1;
    MatrixXf result = MatrixXf::Zero(numRows, numCols);
    result.block(0, 0, numRows, j) = X.block(0, 0, numRows, j);
    result.block(0, j, numRows, numCols - j) = X.block(0, j+1, numRows, numCols-j);
    return result;
};

void change_theta(MatrixXf C, MatrixXf Theta, int j){
    for (int i = 0; i < Theta.cols(); i++){
        if (i != j){
            Theta(i, j) = Theta(j, j) * C(i, j);
        }
    }
};

void symmetrize(MatrixXf Theta, int j){
    for (int i = 0; i < Theta.cols(); i++){
        if (i != j){
            Theta(j, i) = Theta(i, j);
        }
    }
};

MatrixXf bound_below(MatrixXf X){
    for (int i = 0; i < X.rows(); i++){
        for (int j = 0; j < X.cols(); j++){
            if (X(i, j) < 0){
                X(i, j) = 0;
            }
        }
    }
    return X;
}

MatrixXf update_beta_mex(MatrixXf X, MatrixXf S, MatrixXf a,
                     MatrixXf b, float lambda, float gamma, MatrixXf Beta){
    int p = X.cols();
    MatrixXf beta_new = MatrixXf::Zero(p, 1);
    int k, j;
    for (k = 0; k < p; k++) {
        beta_new(k,0) = Beta(k, 0);
    }
    float beta_k, upper_k, lower_k;
    for (k = 0; k < p; k++) {
        beta_k = S(k, 0);
        for (j = 0; j < p; j++) {
            if (j != k)
                beta_k += X(k,j)*beta_new(j, 0);
        }
        beta_k = -1*beta_k/X(k, k);
        upper_k = (gamma*a(k,0) + lambda)/X(k, k);
        lower_k = -1*(gamma*b(k,0) + lambda)/X(k, k);
        if (beta_k > upper_k) {
            beta_new(k,0) = beta_k - upper_k;
        } else if (beta_k < lower_k) {
            beta_new(k,0) = beta_k - lower_k;
        } else {
            beta_new(k,0) = 0;
        }
    }
    return beta_new;
}

MatrixXf optimize_block_coord_sigma(MatrixXf X,
       MatrixXf S, int maxiter, MatrixXf a, MatrixXf b, float lambda,
       float gamma){
    int p = X.cols();
    MatrixXf Beta = MatrixXf::Zero(p, 1);
    MatrixXf oldBeta;
    for (int i = 0; i < maxiter; i++){
        oldBeta = Beta;
        Beta = update_beta_mex(X,S,a,b,lambda,gamma,Beta);
        if ((Beta-oldBeta).norm() < 1e-3) break;
    }
    return -X*(bound_below(Beta)-bound_below(-Beta));
}

MatrixXf optimize_block_coord_beta(MatrixXf X,
    MatrixXf S, int maxiter, MatrixXf a, MatrixXf b, float lambda, float gamma){
    int p = X.cols();
    MatrixXf Beta = MatrixXf::Zero(p, 1);
    MatrixXf oldBeta;
    for (int i = 0; i < maxiter; i++){
        oldBeta = Beta;
        Beta = update_beta_mex(X,S,a,b,lambda,gamma,Beta);
        if ((Beta-oldBeta).norm() < 1e-3) break;
    }
    return Beta;
}

MatrixXf fused_prox_vector(MatrixXf beta, MatrixXf u, MatrixXf l){
    int len = beta.rows();
    MatrixXf w = MatrixXf::Zero(len, 1);
    for (int i = 0; i < len; i++){
        float beta_c = beta(i, 0);
        if (beta_c > u(i, 0)){
            beta(i, 0) = beta_c - u(i, 0);
        } else if (beta_c < l(i, 0)){
            beta(i, 0) = beta_c - l(i, 0);
        }
    }
    return w;

}


float fused_prox_scalar(float beta, float u, float l){
    if (beta > u){
        return beta - u;
    } else if (beta < l){
        return beta - l;
    } else {
        return 0;
    }
}


MatrixXf cwiseAdd(MatrixXf X, float p){
    for (int i = 0; i < X.rows(); i++){
        for (int j = 0; j < X.cols(); j++){
            X(i, j) = X(i, j) + p;
        }
    }
    return X;
}

/* type issues with Q and h_beta */

MatrixXf optimize_block_prox(MatrixXf X,
     MatrixXf s, int maxiter, MatrixXf a, MatrixXf b, float lambda, float gamma){

    //get dimension
    int n = X.rows();

    //initialize variables
    MatrixXf Beta = MatrixXf::Zero(n, 1);
    MatrixXf w = Beta;
    float theta = 1;
    float L = 10;
    MatrixXf objVals = MatrixXf::Zero(maxiter, 1);

    MatrixXf h_w, grad, z, beta_new, upper, lower;
    float h_beta, Q;
    float theta_new;

    //optimize Beta
    for (int iter = 0; iter < maxiter; iter++){
        //computer gradient
        grad = (X*w) + s;

        //compute new estimate of beta using line search
        h_w = w.transpose()*X*w/2 + s.transpose()*w;
        while (true){
            z = w - (1/L)*grad;
            upper = cwiseAdd(gamma*a, lambda)/L;
            lower = -cwiseAdd(gamma*b, lambda)/L;
            beta_new = fused_prox_vector(z,upper,lower);
            h_beta = ((beta_new.transpose()*X*beta_new)/2 + (s.transpose()*beta_new))(0, 0);
            Q = (h_w + (beta_new-w).transpose()*grad)(0,0)+
                L*((beta_new-w).cwiseProduct(beta_new-w)).sum()/2;
            if (h_beta <= Q) break;
            else L = 2*L;

            //update w and theta
            theta_new = (1 + sqrt(1 + 4*theta*theta))/2;
            w = beta_new + (theta-1)/(theta_new)*(beta_new-Beta);

            //store current objective value
            objVals(iter, 0) = h_beta + (cwiseAdd(gamma*a, lambda).transpose()*bound_below(beta_new))(0,0) + 
                               (cwiseAdd(gamma*b, lambda).transpose()*bound_below(-beta_new))(0, 0);

            //store new variables
            Beta = beta_new;
            theta = theta_new;
            if (iter >= 5 && abs(objVals(iter, 0)-objVals(iter-1, 0))/abs(objVals(iter-1, 0)) < 1e-8)
            break;


        }
    }

    //calculate sigma
    MatrixXf sigma = -X*(bound_below(beta_new)-bound_below(-beta_new));
    return sigma;


}



/* end helpers for optimize_theta */

void ICLasso::optimize_theta(){

    //get dimensions ISSUE
    int q = Y.rows();

    //precompute sample covariance & penalty matrices
    MatrixXf S = cov(Y);
    MatrixXf A = MatrixXf::Zero(q, q);
    MatrixXf B = MatrixXf::Zero(q, q);
    for (int j = 0; j < q; j++){
        for (int k = 0; k < q; k++){
            A(j, k) = (Beta.col(j) + Beta.col(k)).cwiseAbs().sum()/2;
            B(j, k) = (Beta.col(j) - Beta.col(k)).cwiseAbs().sum()/2;
        }
        A(j, j) = 0;
        B(j, j) = 0;
    }
    int maxiter = 10000;
    float tol = 1e-4;
    MatrixXf C, s, a, b;

    //store objective values and run times
    MatrixXf objVals = MatrixXf::Zero(maxiter, 1);

    //initialize values;
    MatrixXf Sigma = S + lambda * MatrixXf::Identity(q, q);

    int i = 0;
    //run optimization to obtain estimate of covariance
    for (int iter = 0; iter < maxiter; iter++){
        //iterate through each block
        C = MatrixXf::Zero(q, q);
        for (int j = 0; j < q; j++){
            X = remove_col(remove_row(Sigma, j), j);
            s = remove_row(S, j);
            a = remove_row(A, j);
            b = remove_row(B, j);

            MatrixXf sigma_j, beta;
            sigma_j = optimize_block_coord_sigma(X, s, maxiter, a, b, lambda, gamma);
            beta = optimize_block_coord_beta(X, s, maxiter, a, b, lambda, gamma);
            for (i = 0; i < S.rows(); i++){
                if (i != j) {
                    Sigma(i, j) = sigma_j(i, 0);
                    Sigma(j, i) = sigma_j(i, 0);
                }
            }
            //store beta
            for (i = 0; i < C.rows(); i++){
                if (i != j){
                    C(i, j) = beta(i, 0);
                }
            }
        }
        objVals(iter) = cost_theta(S, Beta, Sigma.inverse(), lambda, gamma);
        //check for convergence 
        if (iter >= 10 && abs(objVals(iter)-objVals(iter-1))/abs(objVals(iter-1)) < tol)
        {break;}

    }




    //calculate inverse covariance
    Theta = MatrixXf::Zero(q, q);
    for (int j = 0; j < q; j++){
        MatrixXf removed_Sigma = remove_row(Sigma, j);
        MatrixXf removed_C = remove_row(C, j);
        Theta(j, j) = 1/(Sigma(j, j) + 
               (removed_Sigma.col(j).transpose() * removed_C.col(j))(0, 0));
        change_theta(C, Theta, j);
        symmetrize(Theta, j);
    }

};