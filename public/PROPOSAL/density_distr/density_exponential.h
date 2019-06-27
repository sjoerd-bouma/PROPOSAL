#pragma once
 
#include "PROPOSAL/density_distr/density_distr.h"
#include "PROPOSAL/math/Vector3D.h"
 
 
class Density_exponential: public Density_distr
{
public:
    Density_exponential(const Axis& axis, double sigma);
 
    ~Density_exponential() {};

    Density_exponential* clone() const {return new Density_exponential(*this);};

    double Integrate(Vector3D xi, Vector3D direction, double res) const;
    double Correct(Vector3D xi, Vector3D direction, double res) const;

    double Calculate(Vector3D xi, Vector3D direction, double distance) const;
        
    double GetDepth(Vector3D xi) const;
    double GetEffectiveDistance(Vector3D xi, Vector3D direction) const;

private:
    Axis* axis_;
    double sigma_;
};


