
#include <functional>

#include <cmath>

#include "PROPOSAL/crossection/IonizIntegral.h"
#include "PROPOSAL/crossection/IonizInterpolant.h"
#include "PROPOSAL/crossection/parametrization/Ionization.h"

#include "PROPOSAL/math/Interpolant.h"
#include "PROPOSAL/math/InterpolantBuilder.h"
#include "PROPOSAL/medium/Components.h"
#include "PROPOSAL/medium/Medium.h"

#include "PROPOSAL/Constants.h"
#include "PROPOSAL/Logging.h"
#include "PROPOSAL/methods.h"

#include "PROPOSAL/particle/ParticleDef.h"

using namespace PROPOSAL;

IonizInterpolant::IonizInterpolant(const Ionization& param, std::shared_ptr<const EnergyCutSettings> cuts, const InterpolationDef& def)
    : CrossSectionInterpolant(param, cuts)
{
    // Use overwritten dNdx interpolation
    InitdNdxInterpolation(def);

    std::cout << "fuu" << std::endl;
    // --------------------------------------------------------------------- //
    // Builder for DEdx
    // --------------------------------------------------------------------- //

    Interpolant1DBuilder builder1d;
    Helper::InterpolantBuilderContainer builder_container;

    std::cout << "bazz" << std::endl;
    IonizIntegral ioniz(param, cuts);

    builder1d.SetMax(def.nodes_cross_section)
        .SetXMin(param.GetParticleMass())
        .SetXMax(def.max_node_energy)
        .SetRomberg(def.order_of_interpolation)
        .SetRational(true)
        .SetRelative(false)
        .SetIsLog(true)
        .SetRombergY(def.order_of_interpolation)
        .SetRationalY(false)
        .SetRelativeY(false)
        .SetLogSubst(true)
        .SetFunction1D(std::bind(&CrossSectionIntegral::CalculatedEdxWithoutMultiplier, &ioniz, std::placeholders::_1));

    builder_container.emplace_back(&builder1d, std::move(dedx_interpolant_));

    // --------------------------------------------------------------------- //
    // Builder for DE2dx
    // --------------------------------------------------------------------- //

    Interpolant1DBuilder builder_de2dx;
    Helper::InterpolantBuilderContainer builder_container_de2dx;

    builder_de2dx.SetMax(def.nodes_continous_randomization)
        .SetXMin(param.GetParticleMass())
        .SetXMax(def.max_node_energy)
        .SetRomberg(def.order_of_interpolation)
        .SetRational(false)
        .SetRelative(false)
        .SetIsLog(true)
        .SetRombergY(def.order_of_interpolation)
        .SetRationalY(false)
        .SetRelativeY(false)
        .SetLogSubst(false)
        .SetFunction1D(std::bind(&IonizIntegral::CalculatedE2dxWithoutMultiplier, &ioniz, std::placeholders::_1));

    builder_container_de2dx.emplace_back(&builder_de2dx, std::move(de2dx_interpolant_));

    Helper::InitializeInterpolation("dEdx", builder_container, std::vector<Parametrization*>(1, parametrization_), def);
    Helper::InitializeInterpolation(
        "dE2dx", builder_container_de2dx, std::vector<Parametrization*>(1, parametrization_), def);
}

/*IonizInterpolant::IonizInterpolant(const IonizInterpolant& ioniz)
    : CrossSectionInterpolant(ioniz)
{
}*/

IonizInterpolant::~IonizInterpolant() {}

// ------------------------------------------------------------------------- //
void IonizInterpolant::InitdNdxInterpolation(const InterpolationDef& def)
{
    // --------------------------------------------------------------------- //
    // Builder for dNdx
    // --------------------------------------------------------------------- //

    std::vector<Interpolant1DBuilder> builder1d(components_.size());
    std::vector<Interpolant2DBuilder> builder2d(components_.size());

    Helper::InterpolantBuilderContainer builder_container1d(components_.size());
    Helper::InterpolantBuilderContainer builder_container2d(components_.size());
    Helper::InterpolantBuilderContainer builder_return;

    Integral integral(IROMB, IMAXS, IPREC);

    for (unsigned int i = 0; i < components_.size(); ++i)
    {
        // !!! IMPORTANT !!!
        // Order of builder matter because the functions needed for 1d interpolation
        // needs the already intitialized 2d interpolants.
        builder2d[i]
            .SetMax1(def.nodes_cross_section)
            .SetX1Min(parametrization_->GetParticleMass())
            .SetX1Max(def.max_node_energy)
            .SetMax2(def.nodes_cross_section)
            .SetX2Min(0.0)
            .SetX2Max(1.0)
            .SetRomberg1(def.order_of_interpolation)
            .SetRational1(false)
            .SetRelative1(false)
            .SetIsLog1(true)
            .SetRomberg2(def.order_of_interpolation)
            .SetRational2(false)
            .SetRelative2(false)
            .SetIsLog2(false)
            .SetRombergY(def.order_of_interpolation)
            .SetRationalY(true)
            .SetRelativeY(false)
            .SetLogSubst(false)
            .SetFunction2D(std::bind(
                &IonizInterpolant::FunctionToBuildDNdxInterpolant2D, this, std::placeholders::_1, std::placeholders::_2, std::ref(integral), i));

        builder_container2d[i].first  = &builder2d[i];
        builder_container2d[i].second = std::move(dndx_interpolant_2d_[i]);

        builder1d[i]
            .SetMax(def.nodes_cross_section)
            .SetXMin(parametrization_->GetParticleMass())
            .SetXMax(def.max_node_energy)
            .SetRomberg(def.order_of_interpolation)
            .SetRational(false)
            .SetRelative(false)
            .SetIsLog(true)
            .SetRombergY(def.order_of_interpolation)
            .SetRationalY(true)
            .SetRelativeY(false)
            .SetLogSubst(false)
            .SetFunction1D(std::bind(&IonizInterpolant::FunctionToBuildDNdxInterpolant, this, std::placeholders::_1, i));

        builder_container1d[i].first  = &builder1d[i];
        builder_container1d[i].second = std::move(dndx_interpolant_1d_[i]);
    }

    for(auto& interpolant2d: builder_container2d){
        builder_return.push_back(std::move(interpolant2d));
    }

    for(auto& interpolant1d: builder_container1d){

        builder_return.push_back(std::move(interpolant1d));
    }

    Helper::InitializeInterpolation("dNdx", builder_return, std::vector<Parametrization*>(1, parametrization_), def);
}

// ----------------------------------------------------------------- //
// Public methods
// ----------------------------------------------------------------- //

double IonizInterpolant::CalculatedEdx(double energy)
{
    if (parametrization_->GetMultiplier() <= 0)
    {
        return 0;
    }

    return parametrization_->GetMultiplier() * std::max(dedx_interpolant_->Interpolate(energy), 0.);
}

// ------------------------------------------------------------------------- //
double IonizInterpolant::CalculatedNdx(double energy)
{
    if (parametrization_->GetMultiplier() <= 0)
    {
        return 0;
    }

    sum_of_rates_ = std::max(dndx_interpolant_1d_[0]->Interpolate(energy), 0.);

    return parametrization_->GetMultiplier() * sum_of_rates_;
}

// ------------------------------------------------------------------------- //
double IonizInterpolant::CalculatedNdx(double energy, double rnd)
{
    (void)rnd;

    if (parametrization_->GetMultiplier() <= 0)
    {
        return 0;
    }

    sum_of_rates_ = std::max(dndx_interpolant_1d_[0]->Interpolate(energy), 0.);

    return parametrization_->GetMultiplier() * sum_of_rates_;
}

// ------------------------------------------------------------------------- //
double IonizInterpolant::FunctionToBuildDNdxInterpolant(double energy, int component)
{
    (void)component;
    return dndx_interpolant_2d_[0]->Interpolate(energy, 1.0);
}

// ------------------------------------------------------------------------- //
double IonizInterpolant::FunctionToBuildDNdxInterpolant2D(double energy, double v, Integral& integral, int component)
{
    (void)component;

    Parametrization::KinematicLimits limits = parametrization_->GetKinematicLimits(energy);

    double vUp = GetEnergyCut(energy);

    if (vUp == limits.vMax)
    {
        return 0;
    }

    v = vUp * std::exp(v * std::log(limits.vMax / vUp));

    return integral.Integrate(
        vUp, v, std::bind(&Parametrization::FunctionToDNdxIntegral, parametrization_, energy, std::placeholders::_1), 3, 1);
}

// ------------------------------------------------------------------------- //
double IonizInterpolant::CalculateStochasticLoss(double energy, double rnd1)
{
    double rnd, rsum;

    rnd  = parametrization_->GetMedium()->GetSumCharge() * rnd1;
    rsum = 0;

    double vUp;

    for (unsigned int i = 0; i < components_.size(); i++)
    {
        rsum += components_[i]->GetAtomInMolecule() * components_[i]->GetNucCharge();

        if (rsum > rnd)
        {
            Parametrization::KinematicLimits limits = parametrization_->GetKinematicLimits(energy);

            vUp = GetEnergyCut(energy);

            if (vUp == limits.vMax)
            {
                return energy * vUp;
            }
            return energy * (vUp * std::exp(dndx_interpolant_2d_[0]->FindLimit(energy, rnd1 * sum_of_rates_) *
                                              std::log(limits.vMax / vUp)));
        }
    }

    log_fatal("SumCharge of medium was not initialized correctly");

    return 0;
}
