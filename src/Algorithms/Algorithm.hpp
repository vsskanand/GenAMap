//
// Created by haohanwang on 1/24/16.
//

#ifndef ALGORITHMS_ALGORITHM_HPP
#define ALGORITHMS_ALGORITHM_HPP

#include <unordered_map>

#ifdef BAZEL
#include "Models/Model.hpp"
#else
#include "../Models/Model.hpp"
#endif

class Algorithm {
protected:
    double progress;
    int maxIteration;
    bool isRunning;
    bool shouldStop;
public:
    Algorithm();
    Algorithm(const unordered_map<string, string>);

    void setMaxIteration(int);

    double getProgress();
    bool getIsRunning();
    int getMaxIteration();
    void stop();
    
    virtual void run(Model*){};
    


    virtual ~Algorithm(){};
};


#endif //ALGORITHMS_ALGORITHM_HPP