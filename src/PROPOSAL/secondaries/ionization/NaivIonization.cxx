#include "PROPOSAL/secondaries/ionization/NaivIonization.h"
#include "PROPOSAL/Constants.h"

#include <cmath>
#include <stdexcept>

using std::fmod;
using std::make_tuple;
using std::sqrt;

using namespace PROPOSAL;



tuple<Vector3D, Vector3D> secondaries::NaivIonization::CalculateDirections(
    Vector3D dir, double, double, double)
{
    return make_tuple(dir, dir);
}

tuple<double, double> secondaries::NaivIonization::CalculateEnergy(
    double energy, double v)
{
    return make_tuple(energy * (1 - v), energy * v);
}

vector<ParticleState> secondaries::NaivIonization::CalculateSecondaries(
    StochasticLoss loss, const Component&, vector<double> &rnd)
{
    auto v = loss.loss_energy / loss.parent_particle_energy;
    auto secondary_energies = CalculateEnergy(loss.parent_particle_energy, v);
    auto secondary_dir = CalculateDirections( loss.direction, loss.loss_energy,
                                              v, rnd[1]);

    auto sec = std::vector<ParticleState>();
    sec.emplace_back(static_cast<ParticleType>(primary_particle_type),
                     loss.position, get<0>(secondary_dir),
                     get<0>(secondary_energies), loss.time, 0.);
    sec.emplace_back(ParticleType::EMinus, loss.position, get<1>(secondary_dir),
                     get<1>(secondary_energies), loss.time, 0.);
    return sec;
}
