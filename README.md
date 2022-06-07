# BlockHammer

Aggressive memory density scaling causes modern DRAM devices to suffer from [`RowHammer`](https://github.com/CMU-SAFARI/rowhammer), a phenomenon where rapidly activating a DRAM row can cause bit-flips in physically-nearby rows. Recent studies [[`1`](https://people.inf.ethz.ch/omutlu/pub/Revisiting-RowHammer_isca20.pdf), [`2`](https://people.inf.ethz.ch/omutlu/pub/rowhammer-TRRespass_ieee_security_privacy20.pdf), [`3`](https://people.inf.ethz.ch/omutlu/pub/U-TRR-uncovering-RowHammer-protection-mechanisms_micro21.pdf)] demonstrate that modern DRAM chips, including chips previously marketed as RowHammer-safe, are even more vulnerable to RowHammer than older chips. Many works show that attackers can exploit RowHammer bit-flips to reliably mount system-level attacks to escalate privilege and leak private data. Therefore, it is critical to ensure RowHammer-safe operation on all DRAM-based systems. 

Unfortunately, state-of-the-art RowHammer mitigation mechanisms face two major challenges. First, they incur increasingly higher performance and/or area overheads when applied to more vulnerable DRAM chips. Second, they require either proprietary information about or modifications to the DRAM chip design. In this paper, we show that it is possible to efficiently and scalably prevent RowHammer bit-flips without knowledge of or modification to DRAM internals. 

We introduce BlockHammer, a low-cost, effective, and easy-to-adopt RowHammer mitigation mechanism that overcomes the two key challenges by selectively throttling memory accesses that could otherwise cause RowHammer bit-flips. The key idea of BlockHammer is to (1) track row activation rates using area-efficient Bloom filters and (2) use the tracking data to ensure that no row is ever activated rapidly enough to induce RowHammer bit-flips. By doing so, BlockHammer (1) makes it impossible for a RowHammer bit-flip to occur and (2) greatly reduces a RowHammer attack's impact on the performance of co-running benign applications. 

Compared to state-of-the-art RowHammer mitigation mechanisms, BlockHammer provides competitive performance and energy when the system is not under a RowHammer attack and significantly better performance and energy when the system is under attack.

The full paper is published in [`HPCA 2021`](https://ieeexplore.ieee.org/abstract/document/9407238) and the pdf is available online: [`arXiv:2102.05981 [cs.CR] `](https://arxiv.org/abs/2102.05981)

# Citation
Please cite our full HPCA 2021 paper if you find this repository useful.

> A. Giray Yaglikci, Minesh Patel, Jeremie S. Kim, Roknoddin Azizi, Ataberk Olgun, Lois Orosa, Hasan Hassan, Jisung Park, Konstantinos Kanellopoulos, Taha Shahroodi, Saugata Ghose, and Onur Mutlu,
[`"BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows"`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer_preventing-DRAM-rowhammer-at-low-cost_hpca21.pdf)
Proceedings of the 27th International Symposium on High-Performance Computer Architecture (HPCA), Virtual, February-March 2021.

```bibtex
@inproceedings{yaglikci2021blockhammer,
  title={{BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows}},
  author={Ya{\u{g}}l{\i}k{\c{c}}{\i}, A Giray and Patel, Minesh and Kim, Jeremie S. and Azizibarzoki, Roknoddin and Olgun, Ataberk and Orosa, Lois and Hassan, Hasan and Park, Jisung and Kanellopoullos, Konstantinos and Shahroodi, Taha and Ghose, Saugata and Mutlu, Onur},
  booktitle={HPCA},
  year={2021}
}
```

# Talks

You can find BlockHammer's talks and slides below:

- [`Talk Video (22 minutes)`](https://www.youtube.com/watch?v=4Y01N1BhWv4&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=102)

  - Slides [`(pptx)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pptx) [`(pdf)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pptx)

- [`Short Talk Video (7 minutes)`](https://www.youtube.com/watch?v=h0WiOTVIH70&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=124)

  - Short Talk Slides [`(pptx)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pptx) [`(pdf)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pdf)

# Getting Started

This repository has two subdirectories. Please refer to each subdirectory on reproducing results. 

- [`Ramulator Model`](ramulator): This subdirectory includes an extended version of [`Ramulator`](https://github.com/CMU-SAFARI/ramulator) with a RowHammerDefense class, which implements BlockHammer along with six state-of-the-art RowHammer mitigaiton mechanisms: 

  | Mechanism  | Reference | 
  |------------|-----------|
  | PARA       | Y. Kim, et al., **''Flipping Bits in Memory Without Accessing Them: An Experimental Study of DRAM Disturbance Errors,''** in ISCA, 2014. |
  | CBT        | S. M. Seyedzadeh et al., **''Mitigating Wordline Crosstalk Using Adaptive Trees of Counters,''** in ISCA, 2018. |
  | ProHIT     | M. Son et al., **''Making DRAM Stronger Against Row Hammering,''** in DAC, 2017. |
  | MrLoc      | J. M. You et al., **''MRLoc: Mitigating Row-Hammering Based on Memory Locality,''** in DAC, 2019.|
  | TWiCe      | E. Lee et al., **''TWiCe: Preventing Row-hammering by Exploiting Time Window Counters''** in ISCA, 2019.|
  | Graphene   | Y. Park et al., **''Graphene: Strong yet Lightweight Row Hammer Protection,''** in MICRO, 2020.|
  

- [`RTL Model`](rtl): This subdirectory includes RTL implementation of the counters and buffers used in BlockHammer.



## Contact
Abdullah Giray Yaglikci (agirayyaglikci@gmail.com)

