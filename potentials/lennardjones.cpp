#include "potentials/lennardjones.h"
#include "celllist.h"
#include "cpelapsedtimer.h"
#include <cmath>
#include <iostream>
using namespace std;
LennardJones::LennardJones(float sigma, float epsilon, float cutoffRadius) :
    m_sigma(sigma),
    m_sigma6(pow(sigma, 6.0)),
    m_epsilon(epsilon),
    m_24epsilon(24*epsilon),
    m_rCutSquared(cutoffRadius*cutoffRadius),
    m_timeSinceLastNeighborListUpdate(0),
    m_potentialEnergyAtRcut(0)
{
    float oneOverDrCut2 = 1.0/m_rCutSquared;
    float oneOverDrCut6 = oneOverDrCut2*oneOverDrCut2*oneOverDrCut2;

    m_potentialEnergyAtRcut = 4*m_epsilon*m_sigma6*oneOverDrCut6*(m_sigma6*oneOverDrCut6 - 1);
}

void LennardJones::calculateForces(System *system)
{
    m_potentialEnergy = 0;
    m_pressureVirial = 0;
    vec3 systemSize = system->systemSize();
    vec3 systemSizeHalf = system->systemSize()*0.5;

    CPElapsedTimer::calculateForces().start();
    CellList &cellList = system->cellList();
    vector<Cell> &cells = cellList.cells();

    for(int cx=0; cx<cellList.numberOfCellsX(); cx++) {
        for(int cy=0; cy<cellList.numberOfCellsY(); cy++) {
            for(int cz=0; cz<cellList.numberOfCellsZ(); cz++) {
                const unsigned int cellIndex1 = cellList.index(cx, cy, cz);
                Cell &cell1 = cells[cellIndex1];
                for(int dx=0; dx<=1; dx++) {
                    for(int dy=(dx==0 ? 0 : -1); dy<=1; dy++) {
                        for(int dz=(dx==0 && dy==0 ? 0 : -1); dz<=1; dz++) {
                            const unsigned int cellIndex2 = cellList.indexPeriodic(cx+dx, cy+dy, cz+dz);
                            Cell &cell2 = cells[cellIndex2];
                            for(unsigned int i=0; i<cell1.numberOfAtoms; i++) {
                                float x = cell1.x[i];
                                float y = cell1.y[i];
                                float z = cell1.z[i];
                                float fix = 0;
                                float fiy = 0;
                                float fiz = 0;
#pragma simd reduction(+: fix, fiy, fiz)
                                for(unsigned int j=(dx==0 && dy==0 && dz==0 ? i+1 : 0); j<cell2.numberOfAtoms; j++) {
                                    float dx = x - cell2.x[j];
                                    float dy = y - cell2.y[j];
                                    float dz = z - cell2.z[j];
#ifdef MINIMUMIMAGECONVENTIONTYPE_BRANCH
                                    if(dx < -systemSizeHalf[0]) dx += systemSize[0];
                                    else if(dx > systemSizeHalf[0]) dx -= systemSize[0];
                                    if(dy < -systemSizeHalf[1]) dy += systemSize[1];
                                    else if(dy > systemSizeHalf[1]) dy -= systemSize[1];
                                    if(dz < -systemSizeHalf[2]) dz += systemSize[2];
                                    else if(dz > systemSizeHalf[2]) dz -= systemSize[2];
#else
                                    dx += systemSize[0]*( (dx < -systemSizeHalf[0] ) - (dx > systemSizeHalf[0]));
                                    dy += systemSize[1]*( (dy < -systemSizeHalf[1] ) - (dy > systemSizeHalf[1]));
                                    dz += systemSize[2]*( (dz < -systemSizeHalf[2] ) - (dz > systemSizeHalf[2]));
#endif

                                    const float dr2 = dx*dx + dy*dy + dz*dz;
                                    const float oneOverDr2 = 1.0f/dr2;
                                    const float sigma6OneOverDr6 = oneOverDr2*oneOverDr2*oneOverDr2*m_sigma6;
                                    const float force = -m_24epsilon*sigma6OneOverDr6*(2*sigma6OneOverDr6 - 1.0f)*oneOverDr2*(dr2 < m_rCutSquared);

                                    fix -= dx*force;
                                    fiy -= dy*force;
                                    fiz -= dz*force;

                                    cell2.fx[j] += dx*force;
                                    cell2.fy[j] += dy*force;
                                    cell2.fz[j] += dz*force;
                                    m_numPairsComputed++;
                                }
                                cell1.fx[i] += fix;
                                cell1.fy[i] += fiy;
                                cell1.fz[i] += fiz;

                            }

                        }}}
            }}}
    CPElapsedTimer::calculateForces().stop();
}

void LennardJones::calculateForcesAndEnergyAndPressure(System *system)
{

    m_potentialEnergy = 0;
    m_pressureVirial = 0;
    vec3 systemSize = system->systemSize();
    vec3 systemSizeHalf = system->systemSize()*0.5;

    CPElapsedTimer::calculateForces().start();
    CellList &cellList = system->cellList();
    vector<Cell> &cells = cellList.cells();

    for(int cx=0; cx<cellList.numberOfCellsX(); cx++) {
        for(int cy=0; cy<cellList.numberOfCellsY(); cy++) {
            for(int cz=0; cz<cellList.numberOfCellsZ(); cz++) {
                const unsigned int cellIndex1 = cellList.index(cx, cy, cz);
                Cell &cell1 = cells[cellIndex1];
                for(int dx=0; dx<=1; dx++) {
                    for(int dy=(dx==0 ? 0 : -1); dy<=1; dy++) {
                        for(int dz=(dx==0 && dy==0 ? 0 : -1); dz<=1; dz++) {
                            const unsigned int cellIndex2 = cellList.indexPeriodic(cx+dx, cy+dy, cz+dz);
                            Cell &cell2 = cells[cellIndex2];
                            const unsigned int cell1Size = cell1.numberOfAtoms;
                            for(unsigned int i=0; i<cell1Size; i++) {
                                float x = cell1.x[i];
                                float y = cell1.y[i];
                                float z = cell1.z[i];
                                float fix = 0;
                                float fiy = 0;
                                float fiz = 0;
                                float pressureVirial = 0;
                                float potentialEnergy = 0;
                                const unsigned int cell2Size = cell2.numberOfAtoms;

#pragma simd reduction(+: fix, fiy, fiz, pressureVirial, potentialEnergy)
                                for(unsigned int j=(dx==0 && dy==0 && dz==0 ? i+1 : 0); j<cell2Size; j++) {
                                    float dx = x - cell2.x[j];
                                    float dy = y - cell2.y[j];
                                    float dz = z - cell2.z[j];
#ifdef MINIMUMIMAGECONVENTIONTYPE_BRANCH
                                    if(dx < -systemSizeHalf[0]) dx += systemSize[0];
                                    else if(dx > systemSizeHalf[0]) dx -= systemSize[0];
                                    if(dy < -systemSizeHalf[1]) dy += systemSize[1];
                                    else if(dy > systemSizeHalf[1]) dy -= systemSize[1];
                                    if(dz < -systemSizeHalf[2]) dz += systemSize[2];
                                    else if(dz > systemSizeHalf[2]) dz -= systemSize[2];
#else
                                    dx += systemSize[0]*( (dx < -systemSizeHalf[0] ) - (dx > systemSizeHalf[0]));
                                    dy += systemSize[1]*( (dy < -systemSizeHalf[1] ) - (dy > systemSizeHalf[1]));
                                    dz += systemSize[2]*( (dz < -systemSizeHalf[2] ) - (dz > systemSizeHalf[2]));
#endif

                                    const float dr2 = dx*dx + dy*dy + dz*dz;
                                    const float oneOverDr2 = 1.0f/dr2;
                                    const float sigma6OneOverDr6 = oneOverDr2*oneOverDr2*oneOverDr2*m_sigma6;
                                    const float force = -m_24epsilon*sigma6OneOverDr6*(2*sigma6OneOverDr6 - 1.0f)*oneOverDr2*(dr2 < m_rCutSquared);

                                    fix -= dx*force;
                                    fiy -= dy*force;
                                    fiz -= dz*force;

                                    cell2.fx[j] += dx*force;
                                    cell2.fy[j] += dy*force;
                                    cell2.fz[j] += dz*force;
                                    pressureVirial += force*sqrt(dr2)*dr2;
                                    potentialEnergy += (4*m_epsilon*sigma6OneOverDr6*(sigma6OneOverDr6 - 1.0f) - m_potentialEnergyAtRcut)*(dr2 < m_rCutSquared);
                                    m_numPairsComputed++;
                                }
                                cell1.fx[i] += fix;
                                cell1.fy[i] += fiy;
                                cell1.fz[i] += fiz;
                                m_pressureVirial += pressureVirial;
                                m_potentialEnergy += potentialEnergy;
                            }

                        }}}
            }}}
    CPElapsedTimer::calculateForces().stop();
}
