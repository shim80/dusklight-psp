#include "dusk/psp/source_getitem_camera.hpp"
#include <cmath>
#include <cstdio>
bool close(float a,float b,float epsilon=0.05f){return std::fabs(a-b)<epsilon;}
int main(){
 using namespace dusk::psp::camera;
 SourceGetItemCamera camera;
 SourceGetItemCameraInput in={
  {1300.0f,207.5f,-2947.5f},{1200.0f,222.5f,-2792.5f},60.0f,
  {1300.0f,62.5f,-2901.5f},{1300.0f,212.5f,-2901.5f},
  static_cast<std::int16_t>(-32768),2,false,false};
 if(!camera.begin(in)||camera.resolved_type()!=3||camera.timer()!=17)return 1;
 for(int i=0;i<17;i++)if(!camera.step())return 2;
 if(!camera.step()||!camera.view().finished)return 3;
 const auto& v=camera.view();
 if(!close(v.center.x,1300)||!close(v.center.y,185.5f)||!close(v.center.z,-2839.5f)||
    !close(v.fov,50.0f,0.01f))return 4;
 std::printf("SOURCE_GETITEM_CAMERA_HOST_OK type=%d timer=%u final_center=%.2f,%.2f,%.2f final_fov=%.2f\n",camera.resolved_type(),camera.timer(),v.center.x,v.center.y,v.center.z,v.fov);
 return 0;
}
