/*
 * pest.cpp - 病虫害预警规则引擎
 */

#include "pest.h"
#include <random>

static random_device rd;
static mt19937 rng(rd());
static uniform_real_distribution<double> uni(0,1);
static double randf(double a,double b){return a+uni(rng)*(b-a);}

PestResult pest_predict(const vector<SensorData>& pts) {
    double avgT=0, avgH=0;
    for(auto& p:pts){avgT+=p.temp; avgH+=p.humidity;}
    avgT/=pts.size(); avgH/=pts.size();

    if(avgT>25 && avgH>70)
        return {"high","霜霉病",65+randf(-10,10),"加强通风，降低田间湿度，喷洒波尔多液预防。"};
    if(avgT>28 && avgH<55)
        return {"high","白粉病",70+randf(-10,10),"喷洒专用杀菌剂，清除病叶残体，防止扩散。"};
    if(avgT>26 && avgH<50)
        return {"medium","蚜虫爆发",60+randf(-10,10),"释放瓢虫天敌或喷洒低毒杀虫剂，控制虫口密度。"};
    if(avgT>27 && avgH<45)
        return {"medium","红蜘蛛",55+randf(-10,10),"检查叶片背面，喷洒杀螨剂并增加田间湿度。"};
    return {"low","无显著风险",randf(3,18),"当前环境良好，无需特殊处理。"};
}
