# RTL Implementation of BlockHammer

We implement this RTL model to evaluate the access latency. Therefore, we model only the largest and most time consuming two structures: Counting Bloom filters and row activation history buffer. 

We use the Synopsys Design Compiler version N-2017.09 to evaluate the access latency, energy, and area costs of BlockHammer's RTL model. In `key-script-command.tcl`, path_to_verilog_files is the path to BlockHammer's verilog source files, `<verilog_module.v>` is the file name containing the verilog module to synthesize, and `<verilog_module_name>` is the name of the module defined in this verilog file.

To evaluate BlockHammer's RTL, you can 1) bring up Synopsys command line (the instructions for this step depends on your system configuration, we execute `icdesign` on the command line and then select Synopsys on the menu) and 2) run `key-script-command.tcl` on the Synopsys command line by typing `source key-script-command.tcl`.

Please refer to the full paper for more information.

> A. Giray Yaglikci, Minesh Patel, Jeremie S. Kim, Roknoddin Azizi, Ataberk Olgun, Lois Orosa, Hasan Hassan, Jisung Park, Konstantinos Kanellopoulos, Taha Shahroodi, Saugata Ghose, and Onur Mutlu,
["BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows"](https://people.inf.ethz.ch/omutlu/pub/BlockHammer_preventing-DRAM-rowhammer-at-low-cost_hpca21.pdf)
in HPCA, 2021.
> 
> [Slides [(pptx)](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pptx) [(pdf)](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pdf)] 
[Short Talk Slides [(pptx)](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pptx) [(pdf)](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pdf)] [[Talk Video (22 minutes)](https://www.youtube.com/watch?v=4Y01N1BhWv4&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=102)] [[Short Talk Video (7 minutes)](https://www.youtube.com/watch?v=h0WiOTVIH70&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=124)]


```bibtex
 @inproceedings{yaglikci2021blockhammer,
  title={{BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows}},
  author={Ya{\u{g}}l{\i}k{\c{c}}{\i}, A Giray and Patel, Minesh and Kim, Jeremie S. and Azizibarzoki, Roknoddin and Olgun, Ataberk and Orosa, Lois and Hassan, Hasan and Park, Jisung and Kanellopoullos, Konstantinos and Shahroodi, Taha and Ghose, Saugata and Mutlu, Onur},
  booktitle={HPCA},
  year={2021}
}
```
