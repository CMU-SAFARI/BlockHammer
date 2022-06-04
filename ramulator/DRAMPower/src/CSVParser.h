#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "Parametrisable.h"
#include "MemorySpecification.h"

#define DRAMPOWER_SPEC_CSV_COL_CNT 5

using namespace std;
using namespace DRAMPower;

/* @Giray: A CSVParameter object represents a parameter defined in Memory specs */
class CSVParameter {
    public:
        int id = -1;
        string name = "";
        string strval = "";
        string datatypestr = "";
        int parent = -1;

        CSVParameter(int id, string name, string strval, string datatypestr, int parent):
        id(id), name(name), strval(strval), datatypestr(datatypestr), parent(parent){
            // switch(datatypestr){
            //     case "int": intval = atoi(strval); break;
            //     case "float": floatval = atof(strval); break;
            //     case "str": break;
            //     default:
            //         cerr << "Unknown data type " << datatypestr << endl;
            //         assert(false);
            // }
        }
};


class CSVParser {
    private:
        //   /**
        //    * Protect people from themselves. This class should never be
        //    * instantiated, nor copied.
        //    */
        //   CSVParser();
        //   CSVParser(const CSVParser&);
        //   CSVParser& operator=(const CSVParser&);

        vector<CSVParameter*> paramlist;
        vector<vector<int> > adjlist;
        map<int, int> csvToInternalIdMapping;
        MemorySpecification memspec;
        // Parametrisable* parameterParent;


    public:
        MemorySpecification getMemorySpecification(){return memspec;}
        MemorySpecification getMemSpecFromCSV(const std::string& id){
            this->parse(id);

            string memtypestr = getStrParameterWithDefault("memoryType", "DDR3");
            memspec.memoryType.set_memtype(memtypestr);
            
            // Setting Memory Architecture Specs
            memspec.memArchSpec.nbrOfBanks = getIntParameterWithDefault("nbrOfBanks",1);
            memspec.memArchSpec.nbrOfRanks = getIntParameterWithDefault("nbrOfRanks",1);
            memspec.memArchSpec.nbrOfBankGroups = getIntParameterWithDefault("nbrOfBankGroups",1);
            memspec.memArchSpec.dataRate = getIntParameterWithDefault("dataRate",1);
            memspec.memArchSpec.burstLength = getIntParameterWithDefault("burstLength",1);
            memspec.memArchSpec.nbrOfColumns = getIntParameterWithDefault("nbrOfColumns",1);
            memspec.memArchSpec.nbrOfRows = getIntParameterWithDefault("nbrOfRows",1);
            memspec.memArchSpec.width = getIntParameterWithDefault("width",1);
            assert("memory width should be a multiple of 8" && (memspec.memArchSpec.width % 8) == 0);
            memspec.memArchSpec.dll = getBoolParameterWithDefault("dll", false);
            memspec.memArchSpec.twoVoltageDomains = getBoolParameterWithDefault("twoVoltageDomains", false);
            memspec.memArchSpec.termination = getBoolParameterWithDefault("termination", false);

            // Setting Memory Power Specs
            memspec.memPowerSpec.idd0 = getDoubleParameterWithDefault("idd0", 0.0);
            memspec.memPowerSpec.idd02 = getDoubleParameterWithDefault("idd02", 0.0);
            memspec.memPowerSpec.idd2p0 = getDoubleParameterWithDefault("idd2p0", 0.0);
            memspec.memPowerSpec.idd2p02 = getDoubleParameterWithDefault("idd2p02", 0.0);
            memspec.memPowerSpec.idd2p1 = getDoubleParameterWithDefault("idd2p1", 0.0);
            memspec.memPowerSpec.idd2p12 = getDoubleParameterWithDefault("idd2p12", 0.0);
            memspec.memPowerSpec.idd2n = getDoubleParameterWithDefault("idd2n", 0.0);
            memspec.memPowerSpec.idd2n2 = getDoubleParameterWithDefault("idd2n2", 0.0);
            memspec.memPowerSpec.idd3p0 = getDoubleParameterWithDefault("idd3p0", 0.0);
            memspec.memPowerSpec.idd3p02 = getDoubleParameterWithDefault("idd3p02", 0.0);
            memspec.memPowerSpec.idd3p1 = getDoubleParameterWithDefault("idd3p1", 0.0);
            memspec.memPowerSpec.idd3p12 = getDoubleParameterWithDefault("idd3p12", 0.0);
            memspec.memPowerSpec.idd3n = getDoubleParameterWithDefault("idd3n", 0.0);
            memspec.memPowerSpec.idd3n2 = getDoubleParameterWithDefault("idd3n2", 0.0);
            memspec.memPowerSpec.idd4r = getDoubleParameterWithDefault("idd4r", 0.0);
            memspec.memPowerSpec.idd4r2 = getDoubleParameterWithDefault("idd4r2", 0.0);
            memspec.memPowerSpec.idd4w = getDoubleParameterWithDefault("idd4w", 0.0);
            memspec.memPowerSpec.idd4w2 = getDoubleParameterWithDefault("idd4w2", 0.0);
            memspec.memPowerSpec.idd5 = getDoubleParameterWithDefault("idd5", 0.0);
            memspec.memPowerSpec.idd52 = getDoubleParameterWithDefault("idd52", 0.0);
            memspec.memPowerSpec.idd5B = getDoubleParameterWithDefault("idd5B", 0.0);
            memspec.memPowerSpec.idd6 = getDoubleParameterWithDefault("idd6", 0.0);
            memspec.memPowerSpec.idd62 = getDoubleParameterWithDefault("idd62", 0.0);
            memspec.memPowerSpec.vdd = getDoubleParameterWithDefault("vdd", 0.0);
            memspec.memPowerSpec.vdd2 = getDoubleParameterWithDefault("vdd2", 0.0);
            memspec.memPowerSpec.capacitance = getDoubleParameterWithDefault("capacitance", 0.0);
            memspec.memPowerSpec.ioPower = getDoubleParameterWithDefault("ioPower", 0.0);
            memspec.memPowerSpec.wrOdtPower = getDoubleParameterWithDefault("wrOdtPower", 0.0);
            memspec.memPowerSpec.termRdPower = getDoubleParameterWithDefault("termRdPower", 0.0);
            memspec.memPowerSpec.termWrPower = getDoubleParameterWithDefault("termWrPower", 0.0);

            // Setting Memory Timing Specs
            memspec.memTimingSpec.clkMhz    = getDoubleParameterWithDefault("clkMhz", 0.0);
            memspec.memTimingSpec.RC        = getIntParameterWithDefault("RC", 0);
            memspec.memTimingSpec.RCD       = getIntParameterWithDefault("RCD", 0);
            memspec.memTimingSpec.CCD       = getIntParameterWithDefault("CCD", 0);
            memspec.memTimingSpec.RRD       = getIntParameterWithDefault("RRD", 0);
            memspec.memTimingSpec.WTR       = getIntParameterWithDefault("WTR", 0);
            memspec.memTimingSpec.CCD_S     = getIntParameterWithDefault("CCD_S", 0);
            memspec.memTimingSpec.CCD_L     = getIntParameterWithDefault("CCD_L", 0);
            memspec.memTimingSpec.RRD_S     = getIntParameterWithDefault("RRD_S", 0);
            memspec.memTimingSpec.RRD_L     = getIntParameterWithDefault("RRD_L", 0);
            memspec.memTimingSpec.WTR_S     = getIntParameterWithDefault("WTR_S", 0);
            memspec.memTimingSpec.WTR_L     = getIntParameterWithDefault("WTR_L", 0);
            memspec.memTimingSpec.TAW       = getIntParameterWithDefault("TAW", 0);
            memspec.memTimingSpec.FAW       = getIntParameterWithDefault("FAW", 0);
            memspec.memTimingSpec.REFI      = getIntParameterWithDefault("REFI", 0);
            memspec.memTimingSpec.RL        = getIntParameterWithDefault("RL", 0);
            memspec.memTimingSpec.RP        = getIntParameterWithDefault("RP", 0);
            memspec.memTimingSpec.RFC       = getIntParameterWithDefault("RFC", 0);
            memspec.memTimingSpec.REFB      = getIntParameterWithDefault("REFB", 0);
            memspec.memTimingSpec.RAS       = getIntParameterWithDefault("RAS", 0);
            memspec.memTimingSpec.WL        = getIntParameterWithDefault("WL", 0);
            memspec.memTimingSpec.AL        = getIntParameterWithDefault("AL", 0);
            memspec.memTimingSpec.DQSCK     = getIntParameterWithDefault("DQSCK", 0);
            memspec.memTimingSpec.RTP       = getIntParameterWithDefault("RTP", 0);
            memspec.memTimingSpec.WR        = getIntParameterWithDefault("WR", 0);
            memspec.memTimingSpec.XP        = getIntParameterWithDefault("XP", 0);
            memspec.memTimingSpec.XPDLL     = getIntParameterWithDefault("XPDLL", 0);
            memspec.memTimingSpec.XS        = getIntParameterWithDefault("XS", 0);
            memspec.memTimingSpec.XSDLL     = getIntParameterWithDefault("XSDLL", 0);
            memspec.memTimingSpec.CKE       = getIntParameterWithDefault("CKE", 0);
            memspec.memTimingSpec.CKESR     = getIntParameterWithDefault("CKESR", 0);
            
            memspec.memTimingSpec.clkPeriod = (memspec.memTimingSpec.clkMhz == 0.0)? 0.0 : 1000.0 / memspec.memTimingSpec.clkMhz;

            // if (memspec.memoryType) {
            memspec.memArchSpec.twoVoltageDomains = memspec.memoryType.hasTwoVoltageDomains();
            memspec.memArchSpec.dll               = memspec.memoryType.hasDll();
            memspec.memArchSpec.termination       = memspec.memoryType.hasTermination();
            memspec.memPowerSpec.capacitance = memspec.memoryType.getCapacitance();
            memspec.memPowerSpec.ioPower     = memspec.memoryType.getIoPower();
            memspec.memPowerSpec.wrOdtPower  = memspec.memoryType.getWrOdtPower();
            memspec.memPowerSpec.termRdPower = memspec.memoryType.getTermRdPower();
            memspec.memPowerSpec.termWrPower = memspec.memoryType.getTermWrPower();
            // }

            return memspec;
        }

        bool getBoolParameterWithDefault(const std::string& id, bool default_val){
            int pindex = findParameter(id);
            if (pindex == -1) return default_val;
            CSVParameter * p = getParameter(id);
            if (p->strval == "true" 
                || p->strval == "True" 
                || p->strval == "1" 
                || p->strval == "on") return true;

            else if (p->strval == "false" 
                || p->strval == "False" 
                || p->strval == "0" 
                || p->strval == "off") return false;

            cerr << "Value " << p->strval << " is not recognized for bool parameter " << p->name << endl;
            assert(false);
        }

        int64_t getIntParameterWithDefault(const std::string& id, int64_t default_val){
            int pindex = findParameter(id);
            if (pindex == -1) return default_val;
            CSVParameter * p = getParameter(id);
            return stol(p->strval);
        }

        double getDoubleParameterWithDefault(const std::string& id, double default_val){
            int pindex = findParameter(id);
            if (pindex == -1) return default_val;
            CSVParameter * p = getParameter(id);
            return stod(p->strval);
        }

        string getStrParameterWithDefault(const std::string& id, string default_val){
            int pindex = findParameter(id);
            if (pindex == -1) return default_val;
            CSVParameter * p = getParameter(id);
            return p->strval;
        }

        int findParameter(const std::string& id){
            for(int i = 0 ; i < (int) paramlist.size() ; i++){
                if (paramlist[(unsigned int)i]->name == id){
                    return i;
                }
            }
            return -1;
        }

        CSVParameter* getParameterWithId(unsigned int id){
            return paramlist[id];
        }

        CSVParameter* getParameter(const std::string& id){
            for(CSVParameter* p : paramlist){
                if (p->name == id){
                    return p;
                }
            }
            cerr << "Parameter " << id << " is missing" << endl;
            assert(false);
        }

        /**
         * General csv parsing function
        */
        void parse(const std::string& filename){
            ifstream file(filename);
            assert(file.good() && "Bad DRAMPower Specs file");
            // format: parameter,value,datatype,subgroup
            string col_str [DRAMPOWER_SPEC_CSV_COL_CNT] = {"id","name", "value", "datatype", "parent"};
            int col_indices [DRAMPOWER_SPEC_CSV_COL_CNT] = {-1, -1, -1, -1};
            bool header_found = false;
            string line;
            int paramId = 0;
            while (getline(file, line)) {
                // cerr << line << endl;
                istringstream iss(line);
                // Parse comma-separated strings
                vector<string> sv;
                istringstream ss(line);
                string token;
                while(getline(ss, token, ',')) {
                    sv.push_back(token);
                }
                if(header_found){
                    int csvId = stoi(sv[(unsigned int)col_indices[0]]);
                    int csvParent =  stoi(sv[(unsigned int)col_indices[4]]);
                    int parent = -1;
                    // cerr << "Here 0" << endl;
                    // cerr << "csvParent: " << csvParent << endl;
                    if (csvParent >= 0 && csvToInternalIdMapping.find(csvParent) != csvToInternalIdMapping.end()){
                        // cerr << "Here 0.0" << endl;
                        parent = csvToInternalIdMapping[csvParent];
                        // cerr << "Here 0.1" << endl;
                        // cerr << "parent: " << parent << endl;
                        adjlist[(unsigned int)parent].push_back(paramId);
                    }
                    // cerr << "Here 1" << endl;
                    paramlist.push_back(new CSVParameter(paramId, sv[(unsigned int)col_indices[1]], 
                    sv[(unsigned int)col_indices[2]], sv[(unsigned int)col_indices[3]], parent));
                    // cerr << "Here 2" << endl;
                    csvToInternalIdMapping[csvId] = paramId;
                    // cerr << "csvToInternalIdMapping[" << csvId << "]: " << csvToInternalIdMapping[csvId] << endl;
                    // cerr << "Here 3" << endl;
                    adjlist.push_back(std::vector<int>());
                    // cerr << "Here 4" << endl;
                    paramId ++;
                }
                else {
                    int col_index = 0;
                    for (auto s : sv){
                        for (int i = 0 ; i < DRAMPOWER_SPEC_CSV_COL_CNT ; i++){
                            if(col_str[i] == s){
                                col_indices[i] = col_index;
                            }
                        }
                        col_index++;
                    }
                    for (int i = 0 ; i < DRAMPOWER_SPEC_CSV_COL_CNT ; i++){
                        // cerr << "Line: " << line << endl;
			// cerr << i << ": " << col_indices[i] << endl;
          		assert(col_indices[i] >= 0);
                    }
                    header_found = true;
                }
            }
            dumpCSVParams();
        }

        void dumpCSVParams(){
            for(auto p : paramlist){
                cout << p->id << ": " << p->name << " " << p->strval << " " << p->datatypestr << " " << p->parent << endl; 
            }
            // for(map<int, vector<int>>::iterator p = csvToInternalIdMapping.begin(); p != csvToInternalIdMapping.end() ; p++){
            //     cout << p->first << ": ";
            //     if(!(p->second.empty())){
            //         for(int c : p.second){
            //             cout << " " << c;
            //         }
            //     }
            //     cout << endl;
            // }
        }
};

#endif
