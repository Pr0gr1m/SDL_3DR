#ifndef SDL1_SIMULATION_H
#define SDL1_SIMULATION_H

class Simulation {
public :
    void Update();

    void *GetDrawCallVerticies();

private:
    void *objectsInScene = nullptr;
    int numObjectsInScene = 0;

    void ApplyGravityToObject();
};

#endif //SDL1_SIMULATION_H
