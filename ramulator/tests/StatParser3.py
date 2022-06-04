#!/usr/bin/python

import os
import re

RUN_TEST = False

class StatParser(object):

    def __init__(self, file_path, verbose=False, is_gem5=False, chosen_stats_index=None):

        self.verbose = verbose
        self.stats_dict_list=[]

        # Check if file exists
        if not os.path.isfile(file_path):
            # print "No file at " + file_path + "!"
            return None

        if self.verbose:
            print("# Parsing " + file_path)

        stats_index = 0
        in_stats = (not is_gem5) and (chosen_stats_index is None )
        with open(file_path) as f:
            vector_stat_name = ""
            for line in f.readlines():
                if "Begin Simulation Statistics" in line:
                    in_stats = (chosen_stats_index is None ) or (chosen_stats_index == stats_index)
                    continue
                elif "End Simulation Statistics" in line:
                    stats_index += 1
                    in_stats = False
                    continue

                if not in_stats:
                    continue
                    
                while (len(self.stats_dict_list) <= stats_index):
                    self.stats_dict_list.append({
                        "stats_file": (file_path, "Path to stats file")
                    })

                lineparts = line.strip().split("#")
                if len(lineparts) > 0:
                    infoparts = lineparts[0].strip().split()
                    desc = safeget(lineparts,1,defval="")
                    name = safeget(infoparts,0)
                    value= float(safeget(infoparts,1))
                    index = re.findall("^\[[0-9]\]", name)
                    if len(index) > 0:
                      name = vector_stat_name+"_"+str(index[0].split("[")[1].split("]")[0]) 
                    else :
                      vector_stat_name = name

                    if self.verbose:
                      print(name, value)
                    if not is_gem5:
                        #all lines start with ramulator so I drop them...
                        name = name.replace("ramulator.","")

                    if name in self.stats_dict_list[stats_index]:
                        if self.verbose:
                            print("#  Repeating stat name: " + name, end=' ')
                            print(" was " + str(self.get_value(name)), end=' ')
                            print(" is " + str(value), end=' ')
                            print(" means " + desc)
                    self.stats_dict_list[stats_index][name] = (value, desc.strip())

    def get_value(self, name):
        retlist = []
        for stats_index, stats_dict in enumerate(self.stats_dict_list):
            if name in stats_dict:
                retlist.append(stats_dict[name][0])
            else :
                if self.verbose:
                    print("#  Could not found " + name + " in stats @ checkpoint", stats_index)
                return -1

        if len(retlist) == 0:
            if self.verbose:
                print("#  Could not found " + name + " in stats")
            return -1

        elif len(retlist) == 1:
            return retlist[0]

        else:
            return retlist

    def get_desc(self, name):
        if name in self.stats_dict_list[0]:
            return self.stats_dict_list[0][name][1]
        else :
            return ""

    def dump_stats(self):
        for stats_dict in self.stats_dict_list:
            for name in stats_dict:
                print(name + ":" + str(self.get_value(name)) + " # " + self.get_desc(name))

def safeget(l, i, defval=False):
    if len(l) > i :
        return l[i]
    else :
        return defval

def StatParserTest():
    stat_parser = StatParser("./DDR3.stats", verbose=True)
    # stat_parser.dump_stats()
    quit()

if RUN_TEST:
    StatParserTest()
