# BlockHammer

Aggressive memory density scaling causes modern DRAM devices to suffer from RowHammer, a phenomenon where rapidly activating a DRAM row can cause bit-flips in physically-nearby rows. Recent studies demonstrate that modern DRAM chips, including chips previously marketed as RowHammer-safe, are even more vulnerable to RowHammer than older chips. Many works show that attackers can exploit RowHammer bit-flips to reliably mount system-level attacks to escalate privilege and leak private data. Therefore, it is critical to ensure RowHammer-safe operation on all DRAM-based systems. Unfortunately, state-of-the-art RowHammer mitigation mechanisms face two major challenges. First, they incur increasingly higher performance and/or area overheads when applied to more vulnerable DRAM chips. Second, they require either proprietary information about or modifications to the DRAM chip design. In this paper, we show that it is possible to efficiently and scalably prevent RowHammer bit-flips without knowledge of or modification to DRAM internals. We introduce BlockHammer, a low-cost, effective, and easy-to-adopt RowHammer mitigation mechanism that overcomes the two key challenges by selectively throttling memory accesses that could otherwise cause RowHammer bit-flips. The key idea of BlockHammer is to (1) track row activation rates using area-efficient Bloom filters and (2) use the tracking data to ensure that no row is ever activated rapidly enough to induce RowHammer bit-flips. By doing so, BlockHammer (1) makes it impossible for a RowHammer bit-flip to occur and (2) greatly reduces a RowHammer attack's impact on the performance of co-running benign applications. Compared to state-of-the-art RowHammer mitigation mechanisms, BlockHammer provides competitive performance and energy when the system is not under a RowHammer attack and significantly better performance and energy when the system is under attack.

# RowBlocker and AttackThrottler
BlockHammer is designed to (1) scale efficiently as DRAM chips become increasingly vulnerable to RowHammer and (2) be compatible with commodity DRAM chips. BlockHammer consists of two components: *RowBlocker* and *AttackThrottler*. 

**RowBlocker**, prevents any possibility of a RowHammer bit-flip by making it impossible to access a DRAM row at a high enough rate to induce RowHammer bit-flips. RowBlocker achieves this by efficiently tracking row activation rates using Bloom filters and throttling the row activations that target rows with high activation rates. We implement RowBlocker entirely within the memory controller, ensuring RowHammer-safe operation without any proprietary information about or modifications to the DRAM chip. Therefore, RowBlocker is compatible with all commodity DRAM chips. 

**AttackThrottler**, alleviates the performance degradation a RowHammer attack can impose upon benign applications by selectively reducing the memory bandwidth usage of only threads that AttackThrottler identifies as likely RowHammer attacks (i.e., attacker threads). By doing so, AttackThrottler provides a larger memory bandwidth to benign applications compared to a baseline system that does not throttle attacker threads. As DRAM chips become more vulnerable to RowHammer, AttackThrottler throttles attacker threads more aggressively, freeing even more memory bandwidth for benign applications to use. By combining RowBlocker and AttackThrottler, BlockHammer achieves both of its design goals.


# Talks

- [`Talk Video (22 minutes)`](https://www.youtube.com/watch?v=4Y01N1BhWv4&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=102)

  - Slides [`(pptx)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pptx) [`(pdf)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-talk.pptx)

- [`Short Talk Video (7 minutes)`](https://www.youtube.com/watch?v=h0WiOTVIH70&list=PL5Q2soXY2Zi8_VVChACnON4sfh2bJ5IrD&index=124)

  - Short Talk Slides [`(pptx)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pptx) [`(pdf)`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer-preventing-rowhammer-at-low-cost-by-blacklisting-rapidly-accessed-dram-rows_hpca21-short-talk.pdf)

# BlockHammer's Implementation

- [`Ramulator Model`](ramulator/README.md)

- [`RTL Model`](rtl/README.md)

# Citation
Please cite our full HPCA 2021 paper if you use this Ramulator version.

> A. Giray Yaglikci, Minesh Patel, Jeremie S. Kim, Roknoddin Azizi, Ataberk Olgun, Lois Orosa, Hasan Hassan, Jisung Park, Konstantinos Kanellopoulos, Taha Shahroodi, Saugata Ghose, and Onur Mutlu,
**"BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows"**
Proceedings of the 27th International Symposium on High-Performance Computer Architecture (HPCA), Virtual, February-March 2021.

```bibtex
@inproceedings{yaglikci2021blockhammer,
  title={{BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows}},
  author={Ya{\u{g}}l{\i}k{\c{c}}{\i}, A Giray and Patel, Minesh and Kim, Jeremie S. and Azizibarzoki, Roknoddin and Olgun, Ataberk and Orosa, Lois and Hassan, Hasan and Park, Jisung and Kanellopoullos, Konstantinos and Shahroodi, Taha and Ghose, Saugata and Mutlu, Onur},
  booktitle={HPCA},
  year={2021}
}
```

## Contact
Abdullah Giray Yaglikci (agirayyaglikci@gmail.com)

