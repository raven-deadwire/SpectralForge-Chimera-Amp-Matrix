#pragma once
namespace spectralforge {
// Fixed-capture input offsets and output contours. Multi-level sine probes
// estimate compression; plucks fit broad tone, separate chords check it.
// Values are broad response corrections, not original circuit coefficients.
// Unmeasured channels are identity. Ironball is retired and excluded.
// Reference lineage, hashes and limits: docs/NATIVE_NAM_CALIBRATION.md.
struct NativeCaptureCalibration { float lowDb{},midDb{},highDb{},levelDb{},inputDb{}; };
inline constexpr NativeCaptureCalibration nativeCaptureCalibration(int model,int channel) noexcept {
    if(model==0 && channel==1)return {-4.50f,-6.00f,6.00f,-4.75f,12.00f};
    if(model==1 && channel==0)return {-6.00f,-6.00f,6.00f,3.22f,0.00f};
    if(model==3 && channel==2)return {1.35f,-6.00f,6.00f,1.33f,3.00f};
    if(model==4 && channel==2)return {0.00f,0.00f,0.00f,0.00f,9.00f};
    if(model==5 && channel==0)return {-2.67f,3.68f,2.76f,-0.94f,0.00f};
    if(model==6 && channel==0)return {-0.86f,-0.79f,4.23f,0.42f,0.00f};
    if(model==7 && channel==0)return {2.29f,-6.00f,6.00f,0.98f,0.00f};
    if(model==8 && channel==1)return {3.54f,0.66f,6.00f,-5.36f,12.00f};
    if(model==9 && channel==1)return {-1.10f,-5.97f,2.09f,1.66f,12.00f};
    if(model==10 && channel==1)return {-6.00f,-6.00f,6.00f,12.32f,-12.00f};
    if(model==11 && channel==0)return {2.18f,-6.00f,6.00f,6.67f,-6.00f};
    if(model==12 && channel==1)return {6.00f,-6.00f,6.00f,3.11f,0.00f};
    if(model==13 && channel==1)return {-6.00f,0.45f,1.16f,0.83f,0.00f};
    if(model==15 && channel==0)return {-3.40f,-2.16f,1.61f,-3.86f,12.00f};
    if(model==15 && channel==1)return {-6.00f,-6.00f,6.00f,-0.92f,12.00f};
    if(model==15 && channel==2)return {-6.00f,-6.00f,6.00f,2.48f,0.00f};
    if(model==15 && channel==3)return {-6.00f,-6.00f,6.00f,2.90f,12.00f};
    if(model==20 && channel==0)return {-6.00f,-6.00f,6.00f,3.02f,0.00f};
    if(model==20 && channel==1)return {-6.00f,-6.00f,6.00f,2.90f,12.00f};
    if(model==21 && channel==0)return {-6.00f,-3.91f,5.52f,2.45f,0.00f};
    if(model==21 && channel==1)return {1.64f,-6.00f,0.56f,1.10f,0.00f};
    if(model==22 && channel==0)return {6.00f,-6.00f,2.70f,7.61f,-12.00f};
    if(model==22 && channel==1)return {6.00f,-4.64f,6.00f,-1.39f,0.00f};
    if(model==2 && channel==1)return {2.93f,-5.95f,5.64f,0.90f,12.00f};
    if(model==23 && channel==0)return {-0.26f,-6.00f,6.00f,1.94f,0.00f};
    if(model==23 && channel==1)return {-6.00f,-6.00f,6.00f,-1.11f,12.00f};
    if(model==23 && channel==2)return {-6.00f,-6.00f,6.00f,0.72f,-6.00f};
    if(model==23 && channel==3)return {-6.00f,-6.00f,6.00f,0.58f,-6.00f};
    if(model==18 && channel==0)return {2.98f,0.30f,4.91f,-0.81f,0.00f};
    if(model==19 && channel==0)return {3.61f,1.69f,6.00f,-4.65f,12.00f};
    if(model==17 && channel==0)return {-6.00f,-5.55f,6.00f,3.04f,0.00f};
    if(model==17 && channel==1)return {6.00f,-6.00f,2.37f,-4.07f,12.00f};
    if(model==17 && channel==2)return {-6.00f,-6.00f,6.00f,2.58f,9.00f};
    if(model==17 && channel==3)return {-6.00f,-6.00f,6.00f,3.74f,-6.00f};
    if(model==20 && channel==2)return {4.05f,-6.00f,6.00f,-6.07f,12.00f};
    return {};
}
} // namespace spectralforge
