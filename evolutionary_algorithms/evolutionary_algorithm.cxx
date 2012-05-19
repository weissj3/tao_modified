#include <iostream>
#include <vector>
#include <string>

#include <stdint.h>

#include "evolutionary_algorithm.hxx"
#include "recombination.hxx"

//from undvc_common
#include "arguments.hxx"

using namespace std;

EvolutionaryAlgorithm::EvolutionaryAlgorithm() {
    current_iteration = 0;
    individuals_created = 0;
    individuals_reported = 0;

    maximum_iterations = 0;
    maximum_created = 0;
    maximum_reported = 0;
}

EvolutionaryAlgorithm::init() {
    Recombination::check_bounds(min_bound, max_bound);
    number_parameters = min_bound.size();
}

EvolutionaryAlgorithm::EvolutionaryAlgorithm( const vector<string> &arguments
                                            ) throw (string) : EvolutionaryAlgorithm() {

    if (!get_argument(arguments, "--population_size", false, population_size)) {
        cerr << "Argument '--population_size' not specified, using default of 50." << endl;
        population_size = 50;
    }

    if (!get_argument(arguments, "--maximum_iterations", false, maximum_iterations)) {
        cerr << "Argument '--maximum_iterations' not specified, could run forever. Hit control-C to quit." << endl;
    }

    if (!get_argument(arguments, "--maximum_created", false, maximum_created)) {
        cerr << "Argument '--maximum_created' not specified, could run forever. Hit control-C to quit." << endl;
    }

    if (!get_argument(arguments, "--maximum_reported", false, maximum_reported)) {
        cerr << "Argument '--maximum_reported' not specified, could run forever. Hit control-C to quit." << endl;
    }

    get_argument_vector<double>(arguments, "--min_bound", true, min_bound);
    get_argument_vector<double>(arguments, "--min_bound", true, max_bound);

    init(min_bound, max_bound);
}

EvolutionaryAlgorithm::EvolutionaryAlgorithm( const vector<double> &min_bound,      /* min bound is copied into the search */
                                              const vector<double> &max_bound,      /* max bound is copied into the search */
                                              const vector<string> &arguments       /* initialize the DE from command line arguments */
                                            ) throw (string) : EvolutionaryAlgorithm(arguments) {

    this->min_bound = vector<double>(min_bound);
    this->max_bound = vector<double>(max_bound);

    init(min_bound, max_bound);
}

EvolutionaryAlgorithm::EvolutionaryAlgorithm( const vector<double> &min_bound,      /* min bound is copied into the search */
                                              const vector<double> &max_bound,      /* max bound is copied into the search */
                                              const uint32_t population_size,
                                              const uint32_t maximum_iterations     /* default value is 0, which means no termination */
                                            ) throw (string) : EvolutionaryAlgorithm() {

    this->population_size = population_size;
    this->maximum_iterations = maximum_iterations;

    this->min_bound = vector<double>(min_bound);
    this->max_bound = vector<double>(max_bound);

    init(min_bound, max_bound);
}

EvolutionaryAlgorithm::EvolutionaryAlgorithm( const vector<double> &min_bound,      /* min bound is copied into the search */
                                              const vector<double> &max_bound,      /* max bound is copied into the search */
                                              const uint32_t population_size,
                                              const uint32_t maximum_created,       /* default value is 0 */
                                              const uint32_t maximum_reported       /* default value is 0 */
                                            ) throw (string) : EvolutionaryAlgorithm {

    this->population_size = population_size;
    this->maximum_created = maximum_created;
    this->maximum_reported = maximum_reported;

    this->min_bound = vector<double>(min_bound);
    this->max_bound = vector<double>(max_bound);

    init(min_bound, max_bound);
}


EvolutionaryAlgorithm::~EvolutionaryAlgorithm() {
}
