#include "Config.h"
#include <unistd.h>
#include <sstream>
#include <string>
#include <getopt.h>

using namespace std;
using namespace ramulator;

Config::Config(const std::string& fname) {
  parse(fname);
}

void Config::parse(const string& fname)
{
    ifstream file(fname);
    assert(file.good() && "Bad config file");
    string line;
    while (getline(file, line)) {
        char delim[] = " \t=";
        vector<string> tokens;

        while (true) {
            size_t start = line.find_first_not_of(delim);
            if (start == string::npos) 
                break;

            size_t end = line.find_first_of(delim, start);
            if (end == string::npos) {
                tokens.push_back(line.substr(start));
                break;
            }

            tokens.push_back(line.substr(start, end - start));
            line = line.substr(end);
        }

        // empty line
        if (!tokens.size())
            continue;

        // comment line
        if (tokens[0][0] == '#')
            continue;

        // parameter line
        assert(tokens.size() == 2 && "Only allow two tokens in one line");

        options[tokens[0]] = tokens[1];
    }
    file.close();
}

static struct option long_options[] = {
            {"mode",     required_argument, 0,  'm' },
            {"stats",  required_argument, 0,  's' },
            {"param",  required_argument, 0,  'p' },
            {"sim-param",  required_argument, 0,  'c' }, // use to specify parameters that will only be applied after warmup
            {"trace",  required_argument, 0,  't' },
            {0,         0,                 0,  0 }
        };

void Config::parse_cmdline(const int argc, char** argv) {

    int option;
    while ((option = getopt_long (argc, argv, "s:m:p:t:c:", long_options, NULL)) != -1) {
        switch(option) {
            case 'c':
            case 'p':{
                std::stringstream ss(optarg);
                std::string item;
                std::vector<std::string> elems;
                while (std::getline(ss, item, '=')) {
                    elems.push_back(std::move(item));
                }
                
                assert(elems.size() == 2 && "Invalid command line argument");

                std::string name = elems[0];
                std::string value = elems[1];

                if(option == 'c') {
                    sim_options[name] = value;
                    break;
                }

                options[name] = value;

                break; }
            case 't':{
                trace_files.push_back(optarg);

                break; }
            case 'm':{
                if(contains("mode"))
                    options["mode"] = optarg;
                else
                    add("mode", optarg);
                break; }
            case 's': {
                if(contains("stats"))
                    options["stats"] = optarg;
                else
                    add("stats", optarg);
                break;}
			case ':':   /* missing option argument */
        		std::cerr << "Error! " << argv[0] << ": option `-" << (char)optopt << "' requires an argument" << std::endl; 
				exit(-1);
        		break;
            case '?':
            default:
        		std::cerr << "Warning! " << argv[0] << ": option `-" << (char)optopt << "' is invalid. Ignored." << std::endl;
				break; 
        }
    }


}
