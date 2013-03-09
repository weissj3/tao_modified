#include <vector>
#include <iostream>
#include <iomanip>
#include <queue>

#include "mpi.h"

#include "asynchronous_algorithms/particle_swarm.hxx"
#include "mpi/mpi_particle_swarm.hxx"

#include "undvc_common/arguments.hxx"

using namespace std;

void master_print() {
    /*
    cout << "[master      ] recv fitness " << setw(10) << fitness << " from " << setw(4) << source << " pos " << individual_position << " [";
    for (int i = 0; i < ParticleSwarm::number_parameters; i++) {
        cout << " " << individual[i];
    }
    cout << "]" << endl;
    */
}

void ParticleSwarmMPI::master() {
    MPI_Status status;

    uint32_t individual_position;
    double individual[ParticleSwarm::number_parameters];

    while (true) {
        //Wait on a message from any worker
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        int n_requested_individuals;
        int source = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
        if (tag == REQUEST_INDIVIDUALS_TAG) {
            MPI_Recv(&n_requested_individuals, 1, MPI_INT, source, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD, &status);
            cout << "worker " << source << " requested: " << n_requested_individuals << " individuals, with a tag: " << tag << ". " << endl;

            for (int i = 0; i < n_requested_individuals; i++) {
                vector<double> new_individual(ParticleSwarm::number_parameters, 0);
                ParticleSwarm::new_individual(individual_position, new_individual);

                MPI_Send(&new_individual[0], ParticleSwarm::number_parameters, MPI_DOUBLE, source, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD);
                MPI_Send(&individual_position, 1, MPI_INT, source, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD);
            }

        } else if (tag == REPORT_FITNESS_TAG) {
            double fitness;
            MPI_Recv(&fitness, 1, MPI_DOUBLE, source, REPORT_FITNESS_TAG, MPI_COMM_WORLD, &status);
            MPI_Recv(individual, ParticleSwarm::number_parameters, MPI_DOUBLE, source, REPORT_FITNESS_TAG, MPI_COMM_WORLD, &status);
            MPI_Recv(&individual_position, 1, MPI_INT, source, REPORT_FITNESS_TAG, MPI_COMM_WORLD, &status);

            vector<double> received_individual(individual, individual + ParticleSwarm::number_parameters);
            ParticleSwarm::insert_individual(individual_position, received_individual, fitness);

            vector<double> new_individual(ParticleSwarm::number_parameters, 0);
            ParticleSwarm::new_individual(individual_position, new_individual);

            MPI_Send(&new_individual[0], ParticleSwarm::number_parameters, MPI_DOUBLE, source, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD);
            MPI_Send(&individual_position, 1, MPI_INT, source, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD);
//            cout << "[master      ] sent individual to " << source << endl;
        } else {
            cerr << "Unknown tag '" << tag << "' received from MPI_Probe on file '" << __FILE__ << "', line " << __LINE__ << endl;
            MPI_Finalize();
            exit(1);
        }

        if (!EvolutionaryAlgorithm::is_running()) {
            //The termination conditions for the search have been met
            for (int i = 0; i < max_rank; i++) {
                //Send a 0 length message because we don't need any contents
                MPI_Send(0, 0, MPI_INT, i, TERMINATE_TAG, MPI_COMM_WORLD);
            }
            return;
        }
    }
}

void worker_print() {
        /*
        cout << "received individual " << individual_position << " [";
        for (int j = 0; j < ParticleSwarm::number_parameters; j++) {
            cout << " " << new_individual->at(j);
        }
        cout << "]" << endl;
        */
}

void ParticleSwarmMPI::worker(double (*objective_function)(const std::vector<double> &)) {
    MPI_Status status;
    int flag;
    int tag;
    int individual_position;
    double individual[ParticleSwarm::number_parameters];
    queue< int > individuals_position_queue;
    queue< vector<double>* > individuals_queue;

    /**
     *  Request enough work to fill the workers queue of individuals
     */
    MPI_Send(&max_queue_size, 1, MPI_INT, 0 /*master is rank 0*/, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD);

    //Fill up the initial queue
    for (int i = 0; i < max_queue_size; i++) {
        MPI_Recv(individual, ParticleSwarm::number_parameters, MPI_DOUBLE, 0 /*master is rank 0*/, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD, &status);
        MPI_Recv(&individual_position, 1, MPI_INT, 0 /*master is rank 0*/, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD, &status);

        vector<double> *new_individual = new vector<double>(individual, individual + ParticleSwarm::number_parameters);

        individuals_queue.push(new_individual);
        individuals_position_queue.push(individual_position);
    }

    long communication_time = 0, communication_start;
    long processing_time = 0, processing_start;

    //Loop forever calculating individual fitness
    communication_start = time(NULL);
    while (true) {
        if (individuals_queue.size() == 0) {
            //The queue is empty, block waiting for a message from the master
            MPI_Probe(0 /*master is rank 0*/, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            flag = 1;
        } else {
            //The queue is not empty, check to see if there's an incoming message from the master
            MPI_Iprobe(0 /*master is rank 0*/, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        }

        while (flag) {
            tag = status.MPI_TAG;
            if (tag == REQUEST_INDIVIDUALS_TAG) {
                //there's an incoming individual, receive it.
                MPI_Recv(individual, ParticleSwarm::number_parameters, MPI_DOUBLE, 0 /*master is rank 0*/, tag, MPI_COMM_WORLD, &status);
                MPI_Recv(&individual_position, 1, MPI_INT, 0 /*master is rank 0*/, REQUEST_INDIVIDUALS_TAG, MPI_COMM_WORLD, &status);

                vector<double> *new_individual = new vector<double>(individual, individual + ParticleSwarm::number_parameters);
                individuals_queue.push(new_individual);
                individuals_position_queue.push(individual_position);
            } else if (tag == TERMINATE_TAG) {
                //Clear out the queue
                while (individuals_queue.size() > 0) {
                    vector<double> *current_individual = individuals_queue.front();
                    individuals_queue.pop();
                    delete current_individual;
                }

                return;
            } else {
                cerr << "[worker " << setw(5) << rank << "] Received unknown tag: " << tag << endl;
                return;
            }

            MPI_Iprobe(0 /*master is rank 0*/, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        }

        //Pop the next individual off the queue
        vector<double> *current_individual = individuals_queue.front();
        individuals_queue.pop();

        int current_individual_position = individuals_position_queue.front();
        individuals_position_queue.pop();

        communication_time += time(NULL) - communication_start;

        //calculate the fitness of the head of the individual queue
        processing_start = time(NULL);
        double fitness = objective_function(*current_individual);
        processing_time += time(NULL) - processing_start;

        communication_start = time(NULL);

        //Send the fitness and the individual back to the master
        MPI_Send(&fitness, 1, MPI_DOUBLE, 0 /*master is rank 0*/, REPORT_FITNESS_TAG, MPI_COMM_WORLD);
        MPI_Send(&(*current_individual)[0], ParticleSwarm::number_parameters, MPI_DOUBLE, 0 /*master is rank 0 */, REPORT_FITNESS_TAG, MPI_COMM_WORLD);
        MPI_Send(&current_individual_position, 1, MPI_INT, 0 /*master is rank 0 */, REPORT_FITNESS_TAG, MPI_COMM_WORLD);

        //Delete the popped individual
        delete current_individual;
    }
}

ParticleSwarmMPI::ParticleSwarmMPI(const std::vector<double> &min_bound,            /* min bound is copied into the search */
                                   const std::vector<double> &max_bound,            /* max bound is copied into the search */
                                   const vector<string> &arguments
                                  ) : ParticleSwarm(min_bound, max_bound, arguments) {

    MPI_Comm_size(MPI_COMM_WORLD, &max_rank);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (!get_argument(arguments, "--max_queue_size", false, max_queue_size)) {
        if (rank == 0) {
            cout << "Argument '--max_queue_size <I>' not found, using default of 3." << endl;
        }
        max_queue_size = 3;
    }
}


void ParticleSwarmMPI::go(double (*objective_function)(const std::vector<double> &)) {
    if (rank == 0) {
        master();
    } else {
        worker(objective_function);
    }
    
    MPI_Finalize();
}

