import pandas as pd
import os
import argparse
from StatParser3 import StatParser

stats = ["cpu_cycles"]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("datadir", type=str, default="./datadir")
    args = parser.parse_args()
    print("Collecting results from", args.datadir)

    df = pd.DataFrame()
    for root, subdirs, files in os.walk(args.datadir):
        if "ramulator.stats" in files:
            # This is a directory that contains exclusively only one simulation's results
            # Parsing the path
            pathparts = root.split("/")
            if len(pathparts) < 2:
                print("Path parsing error:", pathparts)

            sp = StatParser(root+"/ramulator.stats")
            new_result = {
                "path": root,
                "mechanism": pathparts[-1],
                "workload": pathparts[-2],
            }
            
            for stat in stats:
                new_result[stat] = sp.get_value(stat)
            
            df = df.append(new_result, ignore_index=True)
    
    normalized_df = pd.DataFrame()
    for workload, workload_df in df.groupby("workload"):
        print("Dataframe for workload:", workload)
        print(workload_df)
        base_df = workload_df[workload_df.mechanism=="base"]
        print("Non-baseline dataframe for workload:", workload)
        norm_df = workload_df.copy()
        print(norm_df)
        norm_df = norm_df.apply(lambda row: normalize(row, base_df), axis=1)
        print(norm_df)
        normalized_df = normalized_df.append(norm_df, ignore_index=True)
        
    normalized_df.to_csv(args.datadir+"/results.csv")

def normalize(row, base_df):
    for stat in stats:
        if base_df[stat].min() != base_df[stat].max():
            print("There are more than one baseline values!")
            print(base_df)
            quit()
        
        row["norm_"+stat] = row[stat] / base_df[stat].min()
    return row


if __name__ == "__main__":
    main()