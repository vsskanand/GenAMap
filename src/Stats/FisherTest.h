//
// Created by haohanwang on 8/22/16.
//

#ifndef GENAMAP_V2_FISHERTEST_H
#define GENAMAP_V2_FISHERTEST_H

#include "Stats.hpp"

class FisherTest : public StatsBasic {
public:
    void run();
    FisherTest(){shouldCorrect=true;};
    FisherTest(const unordered_map<string, string> &);
};


#endif //GENAMAP_V2_FISHERTEST_H
