<!--
 ! This manual is written in "markdown" format and thus contains some
 ! distracting formatting clutter.  See 'MANUAL' for an easier-to-read version
 ! of this text document, or see the HTML manual online.
 ! -->

Introduction
============

What is HISAT?
-----------------

[HISAT] is a fast and sensitive spliced alignment program.  As part of HISAT, 
we have developed a new indexing scheme based on the Burrows-Wheeler transform ([BWT]) and the [FM index], 
called hierarchical indexing, that employs two types of indexes: 
(1) one global FM index representing the whole genome, and 
(2) many separate local FM indexes for small regions collectively covering the genome.
Our hierarchical index for the human genome (about 3 billion bp) includes ~48,000 local FM indexes, 
each representing a genomic region of ~64,000bp. As the basis for non-gapped alignment, 
the FM index is extremely fast with a low memory footprint, as demonstrated by [Bowtie].
In addition, HISAT provides several alignment strategies specifically
designed for mapping different types of RNA-seq reads. All these together, 
HISAT enables extremely fast and sensitive alignment of reads, 
in particular those spanning two exons or more.  As a result, HISAT is much faster >50 times 
than [TopHat2] with better alignment quality.
Although it uses a large number of indexes, the memory requirement of HISAT is still modest,
approximately 4.3 GB for human.  HISAT uses the [Bowtie2] implementation to handle most of the 
operations on the FM index. In addition to spliced alignment, HISAT handles reads involving indels and
supports a paired-end alignment mode. Multiple processors can be used simultaneously 
to achieve greater alignment speed. HISAT outputs alignments in [SAM] format, 
enabling interoperation with a large number of other tools (e.g. [SAMtools], [GATK]) that use SAM.
HISAT is distributed under the [GPLv3 license], and it runs on the command line under
Linux, Mac OS X and Windows.

[HISAT]:           http://ccb.jhu.edu/software/hisat
[Bowtie2]:        http://bowtie-bio.sf.net/bowtie2
[Bowtie]:          http://bowtie-bio.sf.net
[Bowtie1]:        http://bowtie-bio.sf.net
[Burrows-Wheeler Transform]: http://en.wikipedia.org/wiki/Burrows-Wheeler_transform
[BWT]:             http://en.wikipedia.org/wiki/Burrows-Wheeler_transform
[FM Index]:        http://en.wikipedia.org/wiki/FM-index
[SAM]:             http://samtools.sourceforge.net/SAM1.pdf
[SAMtools]:        http://samtools.sourceforge.net
[GATK]:            http://www.broadinstitute.org/gsa/wiki/index.php/The_Genome_Analysis_Toolkit
[TopHat2]:         http://ccb.jhu.edu/software/tophat
[Cufflinks]:       http://cufflinks.cbcb.umd.edu/
[Crossbow]:        http://bowtie-bio.sf.net/crossbow
[Myrna]:           http://bowtie-bio.sf.net/myrna
[Bowtie paper]:    http://genomebiology.com/2009/10/3/R25
[GPLv3 license]:   http://www.gnu.org/licenses/gpl-3.0.html


Obtaining HISAT
==================

Download HISAT sources and binaries from the Releases sections on the right side.
Binaries are available for Intel architectures (`x86_64`) running Linux, and Mac OS X.

Building from source
--------------------

Building HISAT from source requires a GNU-like environment with GCC, GNU Make
and other basics.  It should be possible to build HISAT on most vanilla Linux
installations or on a Mac installation with [Xcode] installed.  HISAT can
also be built on Windows using [Cygwin] or [MinGW] (MinGW recommended). For a 
MinGW build the choice of what compiler is to be used is important since this
will determine if a 32 or 64 bit code can be successfully compiled using it. If 
there is a need to generate both 32 and 64 bit on the same machine then a multilib 
MinGW has to be properly installed. [MSYS], the [zlib] library, and depending on 
architecture [pthreads] library are also required. We are recommending a 64 bit
build since it has some clear advantages in real life research problems. In order 
to simplify the MinGW setup it might be worth investigating popular MinGW personal 
builds since these are coming already prepared with most of the toolchains needed.

First, download the [source package] from the Releases secion on the right side.
Unzip the file, change to the unzipped directory, and build the
HISAT tools by running GNU `make` (usually with the command `make`, but
sometimes with `gmake`) with no arguments.  If building with MinGW, run `make`
from the MSYS environment.

HISAT is using the multithreading software model in order to speed up 
execution times on SMP architectures where this is possible. On POSIX 
platforms (like linux, Mac OS, etc) it needs the pthread library. Although
it is possible to use pthread library on non-POSIX platform like Windows, due
to performance reasons HISAT will try to use Windows native multithreading
if possible.

For the support of SRA data access in HISAT, please download and install the [NCBI-NGS] toolkit.
When running `make`, specify additional variables as follow.
`make USE_SRA=1 NCBI_NGS_DIR=/path/to/NCBI-NGS-directory NCBI_VDB_DIR=/path/to/NCBI-NGS-directory`,
where `NCBI_NGS_DIR` and `NCBI_VDB_DIR` will be used in Makefile for -I and -L compilation options.
For example, $(NCBI_NGS_DIR)/include and $(NCBI_NGS_DIR)/lib64 will be used.  

[Cygwin]:   http://www.cygwin.com/
[MinGW]:    http://www.mingw.org/
[MSYS]:     http://www.mingw.org/wiki/msys
[zlib]:     http://cygwin.com/packages/mingw-zlib/
[pthreads]: http://sourceware.org/pthreads-win32/
[GnuWin32]: http://gnuwin32.sf.net/packages/coreutils.htm
[Download]: https://sourceforge.net/projects/bowtie-bio/files/bowtie2/
[sourceforge site]: https://sourceforge.net/projects/bowtie-bio/files/bowtie2/
[source package]: http://ccb.jhu.edu/software/hisat/downloads/hisat-0.1.5-beta-source.zip
[Xcode]:    http://developer.apple.com/xcode/
[NCBI-NGS]: https://github.com/ncbi/ngs/wiki/Downloads 

Running HISAT
=============

Adding to PATH
--------------

By adding your new HISAT directory to your [PATH environment variable], you
ensure that whenever you run `hisat`, `hisat-build` or `hisat-inspect`
from the command line, you will get the version you just installed without
having to specify the entire path.  This is recommended for most users.  To do
this, follow your operating system's instructions for adding the directory to
your [PATH].

If you would like to install HISAT by copying the HISAT executable files
to an existing directory in your [PATH], make sure that you copy all the
executables, including `hisat`, `hisat-align-s`, `hisat-align-l`, `hisat-build`, `hisat-build-s`, `hisat-build-l`, `hisat-inspect`, `hisat-inspect-s` and
`hisat-inspect-l`.

[PATH environment variable]: http://en.wikipedia.org/wiki/PATH_(variable)
[PATH]: http://en.wikipedia.org/wiki/PATH_(variable)

Reporting
---------

The reporting mode governs how many alignments HISAT looks for, and how to
report them.

In general, when we say that a read has an alignment, we mean that it has a
[valid alignment].  When we say that a read has multiple alignments, we mean
that it has multiple alignments that are valid and distinct from one another. 

[valid alignment]: #valid-alignments-meet-or-exceed-the-minimum-score-threshold

### Distinct alignments map a read to different places

Two alignments for the same individual read are "distinct" if they map the same
read to different places.  Specifically, we say that two alignments are distinct
if there are no alignment positions where a particular read offset is aligned
opposite a particular reference offset in both alignments with the same
orientation.  E.g. if the first alignment is in the forward orientation and
aligns the read character at read offset 10 to the reference character at
chromosome 3, offset 3,445,245, and the second alignment is also in the forward
orientation and also aligns the read character at read offset 10 to the
reference character at chromosome 3, offset 3,445,245, they are not distinct
alignments.

Two alignments for the same pair are distinct if either the mate 1s in the two
paired-end alignments are distinct or the mate 2s in the two alignments are
distinct or both.

<!--
### Default mode: search for multiple alignments, report the best one

By default, HISAT searches for distinct, valid alignments for each read. When
it finds a valid alignment, it generally will continue to look for alignments
that are nearly as good or better.  It will eventually stop looking, either
because it exceeded a limit placed on search effort (see [`-D`] and [`-R`]) or
because it already knows all it needs to know to report an alignment.
Information from the best alignments are used to estimate mapping quality (the
`MAPQ` [SAM] field) and to set SAM optional fields, such as [`AS:i`] and
[`XS:i`].  HISAT does not gaurantee that the alignment reported is the best
possible in terms of alignment score.
-->

### Default mode: search for one or more alignments, report each

HISAT searches for up to N distinct, primary alignments for
each read, where N equals the integer specified with the `-k` parameter.
Primary alignments mean alignments whose alignment score is equal or higher than any other alignments.
It is possible that multiple distinct alignments whave the same score.
That is, if `-k 2` is specified, HISAT will search for at most 2 distinct
alignments. The alignment score for a paired-end alignment equals the sum of the
alignment scores of the individual mates.  Each reported read or pair alignment
beyond the first has the SAM 'secondary' bit (which equals 256) set in its FLAGS
field.  See the [SAM specification] for details.

HISAT does not "find" alignments in any specific order, so for reads that
have more than N distinct, valid alignments, HISAT does not gaurantee that
the N alignments reported are the best possible in terms of alignment score.
Still, this mode can be effective and fast in situations where the user cares
more about whether a read aligns (or aligns a certain number of times) than
where exactly it originated.


[SAM specification]: http://samtools.sourceforge.net/SAM1.pdf

Alignment summmary
------------------

When HISAT finishes running, it prints messages summarizing what happened. 
These messages are printed to the "standard error" ("stderr") filehandle.  For
datasets consisting of unpaired reads, the summary might look like this:

    20000 reads; of these:
      20000 (100.00%) were unpaired; of these:
        1247 (6.24%) aligned 0 times
        18739 (93.69%) aligned exactly 1 time
        14 (0.07%) aligned >1 times
    93.77% overall alignment rate

For datasets consisting of pairs, the summary might look like this:

    10000 reads; of these:
      10000 (100.00%) were paired; of these:
        650 (6.50%) aligned concordantly 0 times
        8823 (88.23%) aligned concordantly exactly 1 time
        527 (5.27%) aligned concordantly >1 times
        ----
        650 pairs aligned concordantly 0 times; of these:
          34 (5.23%) aligned discordantly 1 time
        ----
        616 pairs aligned 0 times concordantly or discordantly; of these:
          1232 mates make up the pairs; of these:
            660 (53.57%) aligned 0 times
            571 (46.35%) aligned exactly 1 time
            1 (0.08%) aligned >1 times
    96.70% overall alignment rate

The indentation indicates how subtotals relate to totals.

Wrapper
-------

The `hisat`, `hisat-build` and `hisat-inspect` executables are actually 
wrapper scripts that call binary programs as appropriate.  The wrappers shield
users from having to distinguish between "small" and "large" index formats,
discussed briefly in the following section.  Also, the `hisat` wrapper
provides some key functionality, like the ability to handle compressed inputs,
and the fucntionality for [`--un`], [`--al`] and related options.

It is recommended that you always run the hisat wrappers and not run the
binaries directly.

Small and large indexes
-----------------------

`hisat-build` can index reference genomes of any size.  For genomes less than
about 4 billion nucleotides in length, `hisat-build` builds a "small" index
using 32-bit numbers in various parts of the index.  When the genome is longer,
`hisat-build` builds a "large" index using 64-bit numbers.  Small indexes are
stored in files with the `.bt2` extension, and large indexes are stored in
files with the `.bt2l` extension.  The user need not worry about whether a
particular index is small or large; the wrapper scripts will automatically build
and use the appropriate index.

Performance tuning
------------------

1.  If your computer has multiple processors/cores, use `-p`

    The [`-p`] option causes HISAT to launch a specified number of parallel
    search threads.  Each thread runs on a different processor/core and all
    threads find alignments in parallel, increasing alignment throughput by
    approximately a multiple of the number of threads (though in practice,
    speedup is somewhat worse than linear).

Command Line
------------

### Setting function options

Some HISAT options specify a function rather than an individual number or
setting.  In these cases the user specifies three parameters: (a) a function
type `F`, (b) a constant term `B`, and (c) a coefficient `A`.  The available
function types are constant (`C`), linear (`L`), square-root (`S`), and natural
log (`G`). The parameters are specified as `F,B,A` - that is, the function type,
the constant term, and the coefficient are separated by commas with no
whitespace.  The constant term and coefficient may be negative and/or
floating-point numbers.

For example, if the function specification is `L,-0.4,-0.6`, then the function
defined is:

    f(x) = -0.4 + -0.6 * x

If the function specification is `G,1,5.4`, then the function defined is:

    f(x) = 1.0 + 5.4 * ln(x)

See the documentation for the option in question to learn what the parameter `x`
is for.  For example, in the case if the [`--score-min`] option, the function
`f(x)` sets the minimum alignment score necessary for an alignment to be
considered valid, and `x` is the read length.

### Usage

    hisat [options]* -x <hisat-idx> {-1 <m1> -2 <m2> | -U <r> | --sra-acc <SRA accession number>} [-S <hit>]

### Main arguments

<table><tr><td>

[`-x`]: #hisat-options-x

    -x <hisat-idx>

</td><td>

The basename of the index for the reference genome.  The basename is the name of
any of the index files up to but not including the final `.1.bt2` / `.rev.1.bt2`
/ etc.  `hisat` looks for the specified index first in the current directory,
then in the directory specified in the `HISAT_INDEXES` environment variable.

</td></tr><tr><td>

[`-1`]: #hisat-options-1

    -1 <m1>

</td><td>

Comma-separated list of files containing mate 1s (filename usually includes
`_1`), e.g. `-1 flyA_1.fq,flyB_1.fq`.  Sequences specified with this option must
correspond file-for-file and read-for-read with those specified in `<m2>`. Reads
may be a mix of different lengths. If `-` is specified, `hisat` will read the
mate 1s from the "standard in" or "stdin" filehandle.

</td></tr><tr><td>

[`-2`]: #hisat-options-2

    -2 <m2>

</td><td>

Comma-separated list of files containing mate 2s (filename usually includes
`_2`), e.g. `-2 flyA_2.fq,flyB_2.fq`.  Sequences specified with this option must
correspond file-for-file and read-for-read with those specified in `<m1>`. Reads
may be a mix of different lengths. If `-` is specified, `hisat` will read the
mate 2s from the "standard in" or "stdin" filehandle.

</td></tr><tr><td>

[`-U`]: #hisat-options-U

    -U <r>

</td><td>

Comma-separated list of files containing unpaired reads to be aligned, e.g.
`lane1.fq,lane2.fq,lane3.fq,lane4.fq`.  Reads may be a mix of different lengths.
If `-` is specified, `hisat` gets the reads from the "standard in" or "stdin"
filehandle.

</td></tr><tr><td>

[`--sra-acc`]: #hisat-options-sra-acc

    --sra-acc <SRA accession number>

</td><td>

Comma-separated list of SRA accession numbers, e.g. `--sra-acc SRR353653,SRR353654`.

</td></tr><tr><td>

[`-S`]: #hisat-options-S

    -S <hit>

</td><td>

File to write SAM alignments to.  By default, alignments are written to the
"standard out" or "stdout" filehandle (i.e. the console).

</td></tr></table>

### Options

#### Input options

<table>
<tr><td id="hisat-options-q">

[`-q`]: #hisat-options-q

    -q

</td><td>

Reads (specified with `<m1>`, `<m2>`, `<s>`) are FASTQ files.  FASTQ files
usually have extension `.fq` or `.fastq`.  FASTQ is the default format.  See
also: [`--solexa-quals`] and [`--int-quals`].

</td></tr>
<tr><td id="hisat-options-qseq">

[`--qseq`]: #hisat-options-qseq

    --qseq

</td><td>

Reads (specified with `<m1>`, `<m2>`, `<s>`) are QSEQ files.  QSEQ files usually
end in `_qseq.txt`.  See also: [`--solexa-quals`] and [`--int-quals`].

</td></tr>
<tr><td id="hisat-options-f">

[`-f`]: #hisat-options-f

    -f

</td><td>

Reads (specified with `<m1>`, `<m2>`, `<s>`) are FASTA files.  FASTA files
usually have extension `.fa`, `.fasta`, `.mfa`, `.fna` or similar.  FASTA files
do not have a way of specifying quality values, so when `-f` is set, the result
is as if `--ignore-quals` is also set.

</td></tr>
<tr><td id="hisat-options-r">

[`-r`]: #hisat-options-r

    -r

</td><td>

Reads (specified with `<m1>`, `<m2>`, `<s>`) are files with one input sequence
per line, without any other information (no read names, no qualities).  When
`-r` is set, the result is as if `--ignore-quals` is also set.

</td></tr>
<tr><td id="hisat-options-c">

[`-c`]: #hisat-options-c

    -c

</td><td>

The read sequences are given on command line.  I.e. `<m1>`, `<m2>` and
`<singles>` are comma-separated lists of reads rather than lists of read files.
There is no way to specify read names or qualities, so `-c` also implies
`--ignore-quals`.

</td></tr>
<tr><td id="hisat-options-s">

[`-s`/`--skip`]: #hisat-options-s
[`-s`]: #hisat-options-s

    -s/--skip <int>

</td><td>

Skip (i.e. do not align) the first `<int>` reads or pairs in the input.

</td></tr>
<tr><td id="hisat-options-u">

[`-u`/`--qupto`]: #hisat-options-u
[`-u`]: #hisat-options-u

    -u/--qupto <int>

</td><td>

Align the first `<int>` reads or read pairs from the input (after the
[`-s`/`--skip`] reads or pairs have been skipped), then stop.  Default: no limit.

</td></tr>
<tr><td id="hisat-options-5">

[`-5`/`--trim5`]: #hisat-options-5
[`-5`]: #hisat-options-5

    -5/--trim5 <int>

</td><td>

Trim `<int>` bases from 5' (left) end of each read before alignment (default: 0).

</td></tr>
<tr><td id="hisat-options-3">

[`-3`/`--trim3`]: #hisat-options-3
[`-3`]: #hisat-options-3

    -3/--trim3 <int>

</td><td>

Trim `<int>` bases from 3' (right) end of each read before alignment (default:
0).

</td></tr><tr><td id="hisat-options-phred33-quals">

[`--phred33`]: #hisat-options-phred33-quals

    --phred33

</td><td>

Input qualities are ASCII chars equal to the [Phred quality] plus 33.  This is
also called the "Phred+33" encoding, which is used by the very latest Illumina
pipelines.

[Phred quality]: http://en.wikipedia.org/wiki/Phred_quality_score

</td></tr>
<tr><td id="hisat-options-phred64-quals">

[`--phred64`]: #hisat-options-phred64-quals

    --phred64

</td><td>

Input qualities are ASCII chars equal to the [Phred quality] plus 64.  This is
also called the "Phred+64" encoding.

</td></tr>
<tr><td id="hisat-options-solexa-quals">

[`--solexa-quals`]: #hisat-options-solexa-quals

    --solexa-quals

</td><td>

Convert input qualities from [Solexa][Phred quality] (which can be negative) to
[Phred][Phred quality] (which can't).  This scheme was used in older Illumina GA
Pipeline versions (prior to 1.3).  Default: off.

</td></tr>
<tr><td id="hisat-options-int-quals">

[`--int-quals`]: #hisat-options-int-quals

    --int-quals

</td><td>

Quality values are represented in the read input file as space-separated ASCII
integers, e.g., `40 40 30 40`..., rather than ASCII characters, e.g., `II?I`....
 Integers are treated as being on the [Phred quality] scale unless
[`--solexa-quals`] is also specified. Default: off.

</td></tr></table>

#### Alignment options

<table>

<tr><td id="hisat-options-n-ceil">

[`--n-ceil`]: #hisat-options-n-ceil

    --n-ceil <func>

</td><td>

Sets a function governing the maximum number of ambiguous characters (usually
`N`s and/or `.`s) allowed in a read as a function of read length.  For instance,
specifying `-L,0,0.15` sets the N-ceiling function `f` to `f(x) = 0 + 0.15 * x`,
where x is the read length.  See also: [setting function options].  Reads
exceeding this ceiling are [filtered out].  Default: `L,0,0.15`.

[filtered out]: #filtering

</td></tr>

<tr><td id="hisat-options-ignore-quals">

[`--ignore-quals`]: #hisat-options-ignore-quals

    --ignore-quals

</td><td>

When calculating a mismatch penalty, always consider the quality value at the
mismatched position to be the highest possible, regardless of the actual value. 
I.e. input is treated as though all quality values are high.  This is also the
default behavior when the input doesn't specify quality values (e.g. in [`-f`],
[`-r`], or [`-c`] modes).

</td></tr>
<tr><td id="hisat-options-nofw">

[`--nofw`]: #hisat-options-nofw

    --nofw/--norc

</td><td>

If `--nofw` is specified, `hisat` will not attempt to align unpaired reads to
the forward (Watson) reference strand.  If `--norc` is specified, `hisat` will
not attempt to align unpaired reads against the reverse-complement (Crick)
reference strand. In paired-end mode, `--nofw` and `--norc` pertain to the
fragments; i.e. specifying `--nofw` causes `hisat` to explore only those
paired-end configurations corresponding to fragments from the reverse-complement
(Crick) strand.  Default: both strands enabled. 

</td></tr>

<!--
<tr><td id="hisat-options-end-to-end">

[`--end-to-end`]: #hisat-options-end-to-end

    --end-to-end

</td><td>

In this mode, HISAT requires that the entire read align from one end to the
other, without any trimming (or "soft clipping") of characters from either end.
The match bonus [`--ma`] always equals 0 in this mode, so all alignment scores
are less than or equal to 0, and the greatest possible alignment score is 0.
This is mutually exclusive with [`--local`].  `--end-to-end` is the default mode.

</td></tr>
<tr><td id="hisat-options-local">

[`--local`]: #hisat-options-local

    --local

</td><td>

In this mode, HISAT does not require that the entire read align from one end
to the other.  Rather, some characters may be omitted ("soft clipped") from the
ends in order to achieve the greatest possible alignment score.  The match bonus
[`--ma`] is used in this mode, and the best possible alignment score is equal to
the match bonus ([`--ma`]) times the length of the read.  Specifying `--local`
and one of the presets (e.g. `--local --very-fast`) is equivalent to specifying
the local version of the preset (`--very-fast-local`).  This is mutually
exclusive with [`--end-to-end`].  `--end-to-end` is the default mode.

</td></tr>
-->

</table>

#### Scoring options

<table>

<tr><td id="hisat-options-ma">

[`--ma`]: #hisat-options-ma

    --ma <int>

</td><td>

Sets the match bonus.  In [`--local`] mode `<int>` is added to the alignment
score for each position where a read character aligns to a reference character
and the characters match.  Not used in [`--end-to-end`] mode.  Default: 2.

</td></tr>
<tr><td id="hisat-options-mp">

[`--mp`]: #hisat-options-mp

    --mp MX,MN

</td><td>

Sets the maximum (`MX`) and minimum (`MN`) mismatch penalties, both integers.  A
number less than or equal to `MX` and greater than or equal to `MN` is
subtracted from the alignment score for each position where a read character
aligns to a reference character, the characters do not match, and neither is an
`N`.  If [`--ignore-quals`] is specified, the number subtracted quals `MX`.
Otherwise, the number subtracted is `MN + floor( (MX-MN)(MIN(Q, 40.0)/40.0) )`
where Q is the Phred quality value.  Default: `MX` = 6, `MN` = 2.

</td></tr>
<tr><td id="hisat-options-np">

[`--np`]: #hisat-options-np

    --np <int>

</td><td>

Sets penalty for positions where the read, reference, or both, contain an
ambiguous character such as `N`.  Default: 1.

</td></tr>
<tr><td id="hisat-options-rdg">

[`--rdg`]: #hisat-options-rdg

    --rdg <int1>,<int2>

</td><td>

Sets the read gap open (`<int1>`) and extend (`<int2>`) penalties.  A read gap of
length N gets a penalty of `<int1>` + N * `<int2>`.  Default: 5, 3.

</td></tr>
<tr><td id="hisat-options-rfg">

[`--rfg`]: #hisat-options-rfg

    --rfg <int1>,<int2>

</td><td>

Sets the reference gap open (`<int1>`) and extend (`<int2>`) penalties.  A
reference gap of length N gets a penalty of `<int1>` + N * `<int2>`.  Default:
5, 3.

</td></tr>
<tr><td id="hisat-options-score-min">

[`--score-min`]: #hisat-options-score-min

    --score-min <func>

</td><td>

Sets a function governing the minimum alignment score needed for an alignment to
be considered "valid" (i.e. good enough to report).  This is a function of read
length. For instance, specifying `L,0,-0.6` sets the minimum-score function `f`
to `f(x) = 0 + -0.6 * x`, where `x` is the read length.  See also: [setting
function options].  The default is `C,-18,0`.

</td></tr>
</table>

#### Spliced alignment options

<table>

<tr><td id="hisat-options-pen-cansplice">

[`--pen-cansplice`]: #hisat-options-pen-cansplice

    --pen-cansplice <int>

</td><td>

Sets the penalty for each pair of canonical splice sites (e.g. GT/AG). Default: 0.

</td></tr>

<tr><td id="hisat-options-pen-noncansplice">

[`--pen-noncansplice`]: #hisat-options-pen-noncansplice

    --pen-noncansplice <int>

</td><td>

Sets the penalty for each pair of non-canonical splice sites (e.g. non-GT/AG). Default: 3.

</td></tr>
<tr><td id="hisat-options-pen-intronlen">

[`--pen-intronlen`]: #hisat-options-pen-intronlen

    --pen-intronlen <func>

</td><td>

Sets the penalty for long introns so that alignments with shorter introns are preferred
to those with longer ones.  Default: G,-8,1

</td></tr>


<tr><td id="hisat-options-min-intronlen">

[`--min-intronlen`]: #hisat-options-min-intronlen

    --min-intronlen <int>

</td><td>

Sets minimum intron length. Default: 20

</td></tr>


<tr><td id="hisat-options-pen-intronlen">

[`--max-intronlen`]: #hisat-options-max-intronlen

    --max-intronlen <int>

</td><td>

Sets maximum intron length. Default: 500000

</td></tr>

<tr><td id="hisat-options-known-splicesite-infile">

[`--splice-infile`]: #hisat-options-known-splicesite-infile

    --known-splicesite-infile <path>

</td><td>

With this mode, you can provide a list of known splice sites,
which HISAT makes use of to align reads with small anchors.   
You can create such a list using `python extract_splice_sites.py genes.gtf > splicesites.txt`,
where `extract_splice_sites.py` is included in the HISAT package, `genes.gtf` is a gene annotation file,
and `splicesites.txt` is a list of splice sites with which you provide HISAT in this mode.

</td></tr>

<tr><td id="hisat-options-novel-splice-outfile">

[`--novel-splicesite-outfile`]: #hisat-options-novel-splicesite-outfile

    --novel-splicesite-outfile <path>

</td><td>

In this mode, HISAT reports a list of splice sites in the file <path>:  
   chromosome name `<tab>` genomic position of the flanking base on the left side of an intron `<tab>` genomic position of the flanking base on the right `<tab>` strand

</td></tr>

<tr><td id="hisat-options-novel-splicesite-infile">

[`--novel-splicesite-infile`]: #hisat-options-novel-splicesite-infile

    --novel-splicesite-infile <path>

</td><td>

With this mode, you can provide a list of novel splice sites that were generated from the above option "--novel-splicesite-outfile".

</td></tr>

<tr><td id="hisat-options-no-temp-splicesite">

[`--no-temp-splicesite`]: #hisat-options-no-temp-splicesite

    --no-temp-splicesite

</td><td>

HISAT, by default, makes use of splice sites found by earlier reads to align later reads in the same run,
in particular, reads with small anchors (<= 15 bp).  
The option disables this default alignment strategy. 

</td></tr>

<tr><td id="hisat-options-no-spliced-alignment">

[`--no-spliced-alignment`]: #hisat-options-no-spliced-alignment

    --no-spliced-alignment

</td><td>

Disable spliced alignment.

</td></tr>


<tr><td id="hisat-options-rna-strandness">

[`--rna-strandness`]: #hisat-options-rna-strandness

    --rna-strandness <string>

</td><td>

Specify strand-specific information: the default is unstranded.  
For single-end reads, use F or R. 
 'F' means a read corresponds to a transcript.
 'R' means a read corresponds to the reverse complemented counterpart of a transcript.
For paired-end reads, use either FR or RF.  
With this option being used, every read alignment will have an XS attribute tag:
 '+' means a read belongs to a transcript on '+' strand of genome.
 '-' means a read belongs to a transcript on '-' strand of genome.

(TopHat has a similar option, --library-type option, where fr-firststrand corresponds to R and RF; fr-secondstrand corresponds to F and FR.)

</td></tr>

</table>

#### Reporting options

<table>

<tr><td id="hisat-options-k">

[`-k`]: #hisat-options-k

    -k <int>

</td><td>

It searches for at most `<int>` distinct, primary alignments for each read.  Primary alignments mean alignments whose alignment score is equal or higher than any other alignments.
The search terminates when it can't find more distinct valid alignments, or when it
finds `<int>`, whichever happens first. The alignment score for a paired-end
alignment equals the sum of the alignment scores of the individual mates. Each
reported read or pair alignment beyond the first has the SAM 'secondary' bit
(which equals 256) set in its FLAGS field.  For reads that have more than
`<int>` distinct, valid alignments, `hisat` does not gaurantee that the
`<int>` alignments reported are the best possible in terms of alignment score. Default: 5

Note: HISAT is not designed with large values for `-k` in mind, and when
aligning reads to long, repetitive genomes large `-k` can be very, very slow.

</td></tr>

</table>

#### Paired-end options

<table>

<tr><td id="hisat-options-I">

[`-I`/`--minins`]: #hisat-options-I
[`-I`]: #hisat-options-I

    -I/--minins <int>

</td><td>

The minimum fragment length for valid paired-end alignments.  E.g. if `-I 60` is
specified and a paired-end alignment consists of two 20-bp alignments in the
appropriate orientation with a 20-bp gap between them, that alignment is
considered valid (as long as [`-X`] is also satisfied).  A 19-bp gap would not
be valid in that case.  If trimming options [`-3`] or [`-5`] are also used, the
[`-I`] constraint is applied with respect to the untrimmed mates.

The larger the difference between [`-I`] and [`-X`], the slower HISAT will
run.  This is because larger differences bewteen [`-I`] and [`-X`] require that
HISAT scan a larger window to determine if a concordant alignment exists.
For typical fragment length ranges (200 to 400 nucleotides), HISAT is very
efficient.

Default: 0 (essentially imposing no minimum) 

</td></tr>
<tr><td id="hisat-options-X">

[`-X`/`--maxins`]: #hisat-options-X
[`-X`]: #hisat-options-X

    -X/--maxins <int>

</td><td>

The maximum fragment length for valid paired-end alignments.  E.g. if `-X 100`
is specified and a paired-end alignment consists of two 20-bp alignments in the
proper orientation with a 60-bp gap between them, that alignment is considered
valid (as long as [`-I`] is also satisfied).  A 61-bp gap would not be valid in
that case.  If trimming options [`-3`] or [`-5`] are also used, the `-X`
constraint is applied with respect to the untrimmed mates, not the trimmed
mates.

The larger the difference between [`-I`] and [`-X`], the slower HISAT will
run.  This is because larger differences bewteen [`-I`] and [`-X`] require that
HISAT scan a larger window to determine if a concordant alignment exists.
For typical fragment length ranges (200 to 400 nucleotides), HISAT is very
efficient.

Default: 500.

</td></tr>
<tr><td id="hisat-options-fr">

[`--fr`/`--rf`/`--ff`]: #hisat-options-fr
[`--fr`]: #hisat-options-fr
[`--rf`]: #hisat-options-fr
[`--ff`]: #hisat-options-fr

    --fr/--rf/--ff

</td><td>

The upstream/downstream mate orientations for a valid paired-end alignment
against the forward reference strand.  E.g., if `--fr` is specified and there is
a candidate paired-end alignment where mate 1 appears upstream of the reverse
complement of mate 2 and the fragment length constraints ([`-I`] and [`-X`]) are
met, that alignment is valid.  Also, if mate 2 appears upstream of the reverse
complement of mate 1 and all other constraints are met, that too is valid.
`--rf` likewise requires that an upstream mate1 be reverse-complemented and a
downstream mate2 be forward-oriented. ` --ff` requires both an upstream mate 1
and a downstream mate 2 to be forward-oriented.  Default: `--fr` (appropriate
for Illumina's Paired-end Sequencing Assay).

</td></tr>
<tr><td id="hisat-options-no-mixed">

[`--no-mixed`]: #hisat-options-no-mixed

    --no-mixed

</td><td>

By default, when `hisat` cannot find a concordant or discordant alignment for
a pair, it then tries to find alignments for the individual mates.  This option
disables that behavior.

</td></tr>
<tr><td id="hisat-options-no-discordant">

[`--no-discordant`]: #hisat-options-no-discordant

    --no-discordant

</td><td>

By default, `hisat` looks for discordant alignments if it cannot find any
concordant alignments.  A discordant alignment is an alignment where both mates
align uniquely, but that does not satisfy the paired-end constraints
([`--fr`/`--rf`/`--ff`], [`-I`], [`-X`]).  This option disables that behavior.

</td></tr>
<tr><td id="hisat-options-dovetail">

[`--dovetail`]: #hisat-options-dovetail

    --dovetail

</td><td>

If the mates "dovetail", that is if one mate alignment extends past the
beginning of the other such that the wrong mate begins upstream, consider that
to be concordant.  See also: [Mates can overlap, contain or dovetail each
other].  Default: mates cannot dovetail in a concordant alignment.

[Mates can overlap, contain or dovetail each other]: #mates-can-overlap-contain-or-dovetail-each-other

</td></tr>
<tr><td id="hisat-options-no-contain">

[`--no-contain`]: #hisat-options-no-contain

    --no-contain

</td><td>

If one mate alignment contains the other, consider that to be non-concordant.
See also: [Mates can overlap, contain or dovetail each other].  Default: a mate
can contain the other in a concordant alignment.

</td></tr>
<tr><td id="hisat-options-no-overlap">

[`--no-overlap`]: #hisat-options-no-overlap

    --no-overlap

</td><td>

If one mate alignment overlaps the other at all, consider that to be
non-concordant.  See also: [Mates can overlap, contain or dovetail each other]. 
Default: mates can overlap in a concordant alignment.

</td></tr></table>

#### Output options

<table>

<tr><td id="hisat-options-t">

[`-t`/`--time`]: #hisat-options-t
[`-t`]: #hisat-options-t

    -t/--time

</td><td>

Print the wall-clock time required to load the index files and align the reads. 
This is printed to the "standard error" ("stderr") filehandle.  Default: off.

</td></tr>
<tr><td id="hisat-options-un">

[`--un`]: #hisat-options-un
[`--un-gz`]: #hisat-options-un
[`--un-bz2`]: #hisat-options-un

    --un <path>
    --un-gz <path>
    --un-bz2 <path>

</td><td>

Write unpaired reads that fail to align to file at `<path>`.  These reads
correspond to the SAM records with the FLAGS `0x4` bit set and neither the
`0x40` nor `0x80` bits set.  If `--un-gz` is specified, output will be gzip
compressed. If `--un-bz2` is specified, output will be bzip2 compressed.  Reads
written in this way will appear exactly as they did in the input file, without
any modification (same sequence, same name, same quality string, same quality
encoding).  Reads will not necessarily appear in the same order as they did in
the input.

</td></tr>
<tr><td id="hisat-options-al">

[`--al`]: #hisat-options-al
[`--al-gz`]: #hisat-options-al
[`--al-bz2`]: #hisat-options-al

    --al <path>
    --al-gz <path>
    --al-bz2 <path>

</td><td>

Write unpaired reads that align at least once to file at `<path>`.  These reads
correspond to the SAM records with the FLAGS `0x4`, `0x40`, and `0x80` bits
unset.  If `--al-gz` is specified, output will be gzip compressed. If `--al-bz2`
is specified, output will be bzip2 compressed.  Reads written in this way will
appear exactly as they did in the input file, without any modification (same
sequence, same name, same quality string, same quality encoding).  Reads will
not necessarily appear in the same order as they did in the input.

</td></tr>
<tr><td id="hisat-options-un-conc">

[`--un-conc`]: #hisat-options-un-conc
[`--un-conc-gz`]: #hisat-options-un-conc
[`--un-conc-bz2`]: #hisat-options-un-conc

    --un-conc <path>
    --un-conc-gz <path>
    --un-conc-bz2 <path>

</td><td>

Write paired-end reads that fail to align concordantly to file(s) at `<path>`.
These reads correspond to the SAM records with the FLAGS `0x4` bit set and
either the `0x40` or `0x80` bit set (depending on whether it's mate #1 or #2).
`.1` and `.2` strings are added to the filename to distinguish which file
contains mate #1 and mate #2.  If a percent symbol, `%`, is used in `<path>`,
the percent symbol is replaced with `1` or `2` to make the per-mate filenames.
Otherwise, `.1` or `.2` are added before the final dot in `<path>` to make the
per-mate filenames.  Reads written in this way will appear exactly as they did
in the input files, without any modification (same sequence, same name, same
quality string, same quality encoding).  Reads will not necessarily appear in
the same order as they did in the inputs.

</td></tr>
<tr><td id="hisat-options-al-conc">

[`--al-conc`]: #hisat-options-al-conc
[`--al-conc-gz`]: #hisat-options-al-conc
[`--al-conc-bz2`]: #hisat-options-al-conc

    --al-conc <path>
    --al-conc-gz <path>
    --al-conc-bz2 <path>

</td><td>

Write paired-end reads that align concordantly at least once to file(s) at
`<path>`. These reads correspond to the SAM records with the FLAGS `0x4` bit
unset and either the `0x40` or `0x80` bit set (depending on whether it's mate #1
or #2). `.1` and `.2` strings are added to the filename to distinguish which
file contains mate #1 and mate #2.  If a percent symbol, `%`, is used in
`<path>`, the percent symbol is replaced with `1` or `2` to make the per-mate
filenames. Otherwise, `.1` or `.2` are added before the final dot in `<path>` to
make the per-mate filenames.  Reads written in this way will appear exactly as
they did in the input files, without any modification (same sequence, same name,
same quality string, same quality encoding).  Reads will not necessarily appear
in the same order as they did in the inputs.

</td></tr>
<tr><td id="hisat-options-quiet">

[`--quiet`]: #hisat-options-quiet

    --quiet

</td><td>

Print nothing besides alignments and serious errors.

</td></tr>
<tr><td id="hisat-options-met-file">

[`--met-file`]: #hisat-options-met-file

    --met-file <path>

</td><td>

Write `hisat` metrics to file `<path>`.  Having alignment metric can be useful
for debugging certain problems, especially performance issues.  See also:
[`--met`].  Default: metrics disabled.

</td></tr>
<tr><td id="hisat-options-met-stderr">

[`--met-stderr`]: #hisat-options-met-stderr

    --met-stderr

</td><td>

Write `hisat` metrics to the "standard error" ("stderr") filehandle.  This is
not mutually exclusive with [`--met-file`].  Having alignment metric can be
useful for debugging certain problems, especially performance issues.  See also:
[`--met`].  Default: metrics disabled.

</td></tr>
<tr><td id="hisat-options-met">

[`--met`]: #hisat-options-met

    --met <int>

</td><td>

Write a new `hisat` metrics record every `<int>` seconds.  Only matters if
either [`--met-stderr`] or [`--met-file`] are specified.  Default: 1.

</td></tr>
</table>

#### SAM options

<table>

<tr><td id="hisat-options-no-unal">

[`--no-unal`]: #hisat-options-no-unal

    --no-unal

</td><td>

Suppress SAM records for reads that failed to align.

</td></tr>
<tr><td id="hisat-options-no-hd">

[`--no-hd`]: #hisat-options-no-hd

    --no-hd

</td><td>

Suppress SAM header lines (starting with `@`).

</td></tr>
<tr><td id="hisat-options-no-sq">

[`--no-sq`]: #hisat-options-no-sq

    --no-sq

</td><td>

Suppress `@SQ` SAM header lines.

</td></tr>
<tr><td id="hisat-options-rg-id">

[`--rg-id`]: #hisat-options-rg-id

    --rg-id <text>

</td><td>

Set the read group ID to `<text>`.  This causes the SAM `@RG` header line to be
printed, with `<text>` as the value associated with the `ID:` tag.  It also
causes the `RG:Z:` extra field to be attached to each SAM output record, with
value set to `<text>`.

</td></tr>
<tr><td id="hisat-options-rg">

[`--rg`]: #hisat-options-rg

    --rg <text>

</td><td>

Add `<text>` (usually of the form `TAG:VAL`, e.g. `SM:Pool1`) as a field on the
`@RG` header line.  Note: in order for the `@RG` line to appear, [`--rg-id`]
must also be specified.  This is because the `ID` tag is required by the [SAM
Spec][SAM].  Specify `--rg` multiple times to set multiple fields.  See the
[SAM Spec][SAM] for details about what fields are legal.


</td></tr>
<tr><td id="hisat-options-omit-sec-seq">

[`--omit-sec-seq`]: #hisat-options-omit-sec-seq

    --omit-sec-seq

</td><td>

When printing secondary alignments, HISAT by default will write out the `SEQ`
and `QUAL` strings.  Specifying this option causes HISAT to print an asterix
in those fields instead.

</td></tr>


</table>

#### Performance options

<table><tr>

<td id="hisat-options-o">

[`-o`/`--offrate`]: #hisat-options-o
[`-o`]: #hisat-options-o
[`--offrate`]: #hisat-options-o

    -o/--offrate <int>

</td><td>

Override the offrate of the index with `<int>`.  If `<int>` is greater
than the offrate used to build the index, then some row markings are
discarded when the index is read into memory.  This reduces the memory
footprint of the aligner but requires more time to calculate text
offsets.  `<int>` must be greater than the value used to build the
index.

</td></tr>
<tr><td id="hisat-options-p">

[`-p`/`--threads`]: #hisat-options-p
[`-p`]: #hisat-options-p

    -p/--threads NTHREADS

</td><td>

Launch `NTHREADS` parallel search threads (default: 1).  Threads will run on
separate processors/cores and synchronize when parsing reads and outputting
alignments.  Searching for alignments is highly parallel, and speedup is close
to linear.  Increasing `-p` increases HISAT's memory footprint. E.g. when
aligning to a human genome index, increasing `-p` from 1 to 8 increases the
memory footprint by a few hundred megabytes.  This option is only available if
`bowtie` is linked with the `pthreads` library (i.e. if `BOWTIE_PTHREADS=0` is
not specified at build time).

</td></tr>
<tr><td id="hisat-options-reorder">

[`--reorder`]: #hisat-options-reorder

    --reorder

</td><td>

Guarantees that output SAM records are printed in an order corresponding to the
order of the reads in the original input file, even when [`-p`] is set greater
than 1.  Specifying `--reorder` and setting [`-p`] greater than 1 causes HISAT
to run somewhat slower and use somewhat more memory then if `--reorder` were
not specified.  Has no effect if [`-p`] is set to 1, since output order will
naturally correspond to input order in that case.

</td></tr>
<tr><td id="hisat-options-mm">

[`--mm`]: #hisat-options-mm

    --mm

</td><td>

Use memory-mapped I/O to load the index, rather than typical file I/O.
Memory-mapping allows many concurrent `bowtie` processes on the same computer to
share the same memory image of the index (i.e. you pay the memory overhead just
once).  This facilitates memory-efficient parallelization of `bowtie` in
situations where using [`-p`] is not possible or not preferable.

</td></tr></table>

#### Other options

<table>
<tr><td id="hisat-options-qc-filter">

[`--qc-filter`]: #hisat-options-qc-filter

    --qc-filter

</td><td>

Filter out reads for which the QSEQ filter field is non-zero.  Only has an
effect when read format is [`--qseq`].  Default: off.

</td></tr>
<tr><td id="hisat-options-seed">

[`--seed`]: #hisat-options-seed

    --seed <int>

</td><td>

Use `<int>` as the seed for pseudo-random number generator.  Default: 0.

</td></tr>
<tr><td id="hisat-options-non-deterministic">

[`--non-deterministic`]: #hisat-options-non-deterministic

    --non-deterministic

</td><td>

Normally, HISAT re-initializes its pseudo-random generator for each read.  It
seeds the generator with a number derived from (a) the read name, (b) the
nucleotide sequence, (c) the quality sequence, (d) the value of the [`--seed`]
option.  This means that if two reads are identical (same name, same
nucleotides, same qualities) HISAT will find and report the same alignment(s)
for both, even if there was ambiguity.  When `--non-deterministic` is specified,
HISAT re-initializes its pseudo-random generator for each read using the
current time.  This means that HISAT will not necessarily report the same
alignment for two identical reads.  This is counter-intuitive for some users,
but might be more appropriate in situations where the input consists of many
identical reads.

</td></tr>
<tr><td id="hisat-options-version">

[`--version`]: #hisat-options-version

    --version

</td><td>

Print version information and quit.

</td></tr>
<tr><td id="hisat-options-h">

    -h/--help

</td><td>

Print usage information and quit.

</td></tr></table>

SAM output
----------

Following is a brief description of the [SAM] format as output by `hisat`. 
For more details, see the [SAM format specification][SAM].

By default, `hisat` prints a SAM header with `@HD`, `@SQ` and `@PG` lines. 
When one or more [`--rg`] arguments are specified, `hisat` will also print
an `@RG` line that includes all user-specified [`--rg`] tokens separated by
tabs.

Each subsequnt line describes an alignment or, if the read failed to align, a
read.  Each line is a collection of at least 12 fields separated by tabs; from
left to right, the fields are:

1.  Name of read that aligned.

    Note that the [SAM specification] disallows whitespace in the read name.
	If the read name contains any whitespace characters, HISAT will truncate
	the name at the first whitespace character.  This is similar to the
	behavior of other tools.

2.  Sum of all applicable flags.  Flags relevant to HISAT are:

    <table><tr><td>

        1

    </td><td>

    The read is one of a pair

    </td></tr><tr><td>

        2

    </td><td>

    The alignment is one end of a proper paired-end alignment

    </td></tr><tr><td>

        4

    </td><td>

    The read has no reported alignments

    </td></tr><tr><td>

        8

    </td><td>

    The read is one of a pair and has no reported alignments

    </td></tr><tr><td>

        16

    </td><td>

    The alignment is to the reverse reference strand

    </td></tr><tr><td>

        32

    </td><td>

    The other mate in the paired-end alignment is aligned to the
    reverse reference strand

    </td></tr><tr><td>

        64

    </td><td>

    The read is mate 1 in a pair

    </td></tr><tr><td>

        128

    </td><td>

    The read is mate 2 in a pair

    </td></tr></table>

    Thus, an unpaired read that aligns to the reverse reference strand
    will have flag 16.  A paired-end read that aligns and is the first
    mate in the pair will have flag 83 (= 64 + 16 + 2 + 1).

3.  Name of reference sequence where alignment occurs

4.  1-based offset into the forward reference strand where leftmost
    character of the alignment occurs

5.  Mapping quality

6.  CIGAR string representation of alignment

7.  Name of reference sequence where mate's alignment occurs.  Set to `=` if the
mate's reference sequence is the same as this alignment's, or `*` if there is no
mate.

8.  1-based offset into the forward reference strand where leftmost character of
the mate's alignment occurs.  Offset is 0 if there is no mate.

9.  Inferred fragment length.  Size is negative if the mate's alignment occurs
upstream of this alignment.  Size is 0 if the mates did not align concordantly.
However, size is non-0 if the mates aligned discordantly to the same
chromosome.

10. Read sequence (reverse-complemented if aligned to the reverse strand)

11. ASCII-encoded read qualities (reverse-complemented if the read aligned to
the reverse strand).  The encoded quality values are on the [Phred quality]
scale and the encoding is ASCII-offset by 33 (ASCII char `!`), similarly to a
[FASTQ] file.

12. Optional fields.  Fields are tab-separated.  `hisat` outputs zero or more
of these optional fields for each alignment, depending on the type of the
alignment:

    <table>
    <tr><td id="hisat-build-opt-fields-as">

        AS:i:<N>

    </td>
    <td>

    Alignment score.  Can be negative.  Can be greater than 0 in [`--local`]
    mode (but not in [`--end-to-end`] mode).  Only present if SAM record is for
    an aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-ys">

        YS:i:<N>

    </td>
    <td>

    Alignment score for opposite mate in the paired-end alignment.  Only present
    if the SAM record is for a read that aligned as part of a paired-end
    alignment.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-xn">

        XN:i:<N>

    </td>
    <td>

    The number of ambiguous bases in the reference covering this alignment. 
    Only present if SAM record is for an aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-xm">

        XM:i:<N>

    </td>
    <td>

    The number of mismatches in the alignment.  Only present if SAM record is
    for an aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-xo">

        XO:i:<N>

    </td>
    <td>

    The number of gap opens, for both read and reference gaps, in the alignment.
    Only present if SAM record is for an aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-xg">

        XG:i:<N>

    </td>
    <td>

    The number of gap extensions, for both read and reference gaps, in the
    alignment. Only present if SAM record is for an aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-nm">

        NM:i:<N>

    </td>
    <td>

    The edit distance; that is, the minimal number of one-nucleotide edits
    (substitutions, insertions and deletions) needed to transform the read
    string into the reference string.  Only present if SAM record is for an
    aligned read.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-yf">

        YF:Z:<S>

    </td><td>

    String indicating reason why the read was filtered out.  See also:
    [Filtering].  Only appears for reads that were filtered out.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-yt">

        YT:Z:<S>

    </td><td>

    Value of `UU` indicates the read was not part of a pair.  Value of `CP`
    indicates the read was part of a pair and the pair aligned concordantly.
    Value of `DP` indicates the read was part of a pair and the pair aligned
    discordantly.  Value of `UP` indicates the read was part of a pair but the
    pair failed to aligned either concordantly or discordantly.

    </td></tr>
    <tr><td id="hisat-build-opt-fields-md">

        MD:Z:<S>

    </td><td>

    A string representation of the mismatched reference bases in the alignment. 
    See [SAM] format specification for details.  Only present if SAM record is
    for an aligned read.

    </td></tr>
    <tr><td id="hisat-opt-fields-xs">

        XS:A:<A>

    </td><td>

    Values of `+` and `-` indicate the read is mapped to transcripts on sense and anti-sense
    strands, respectively.  Spliced alignments need to have this field, which is required in Cufflinks and StringTie.  
    We can report this field for the canonical-splice site (GT/AG), but not for non-canonical splice sites.
    You can direct HISAT not to output such alignments (involving non-canonical splice sites)  using "--pen-noncansplice 1000000".

    </td></tr>
    <tr><td id="hisat-opt-fields-nh">

        NH:i:<N>

    </td><td>

    The number of mapped locations for the read or the pair.
    </td></tr>
    
    </table>

[SAM format specification]: http://samtools.sf.net/SAM1.pdf
[FASTQ]: http://en.wikipedia.org/wiki/FASTQ_format
[`-S`/`--sam`]: #hisat-options-S
[`-m`]: #hisat-options-m

The `hisat-build` indexer
===========================

`hisat-build` builds a HISAT index from a set of DNA sequences.
`hisat-build` outputs a set of 6 files with suffixes `.1.bt2`, `.2.bt2`,
`.3.bt2`, `.4.bt2`, `.rev.1.bt2`, and `.rev.2.bt2`.  In the case of a large 
index these suffixes will have a `bt2l` termination.  These files together
constitute the index: they are all that is needed to align reads to that
reference.  The original sequence FASTA files are no longer used by HISAT
once the index is built.

Use of Karkkainen's [blockwise algorithm] allows `hisat-build` to trade off
between running time and memory usage. `hisat-build` has three options
governing how it makes this trade: [`-p`/`--packed`], [`--bmax`]/[`--bmaxdivn`],
and [`--dcv`].  By default, `hisat-build` will automatically search for the
settings that yield the best running time without exhausting memory.  This
behavior can be disabled using the [`-a`/`--noauto`] option.

The indexer provides options pertaining to the "shape" of the index, e.g.
[`--offrate`](#hisat-build-options-o) governs the fraction of [Burrows-Wheeler]
rows that are "marked" (i.e., the density of the suffix-array sample; see the
original [FM Index] paper for details).  All of these options are potentially
profitable trade-offs depending on the application.  They have been set to
defaults that are reasonable for most cases according to our experiments.  See
[Performance tuning] for details.

`hisat-build` can generate either [small or large indexes](#small-and-large-indexes).  The wrapper
will decide which based on the length of the input genome.  If the reference
does not exceed 4 billion characters but a large index is preferred,  the user
can specify [`--large-index`] to force `hisat-build` to build a large index
instead.

The HISAT index is based on the [FM Index] of Ferragina and Manzini, which in
turn is based on the [Burrows-Wheeler] transform.  The algorithm used to build
the index is based on the [blockwise algorithm] of Karkkainen.

[Blockwise algorithm]: http://portal.acm.org/citation.cfm?id=1314852
[Burrows-Wheeler]: http://en.wikipedia.org/wiki/Burrows-Wheeler_transform
[Performance tuning]: #performance-tuning

Command Line
------------

Usage:

    hisat-build [options]* <reference_in> <bt2_base>

### Main arguments

<table><tr><td>

    <reference_in>

</td><td>

A comma-separated list of FASTA files containing the reference sequences to be
aligned to, or, if [`-c`](#hisat-build-options-c) is specified, the sequences
themselves. E.g., `<reference_in>` might be `chr1.fa,chr2.fa,chrX.fa,chrY.fa`,
or, if [`-c`](#hisat-build-options-c) is specified, this might be
`GGTCATCCT,ACGGGTCGT,CCGTTCTATGCGGCTTA`.

</td></tr><tr><td>

    <bt2_base>

</td><td>

The basename of the index files to write.  By default, `hisat-build` writes
files named `NAME.1.bt2`, `NAME.2.bt2`, `NAME.3.bt2`, `NAME.4.bt2`,
`NAME.5.bt2`, `NAME.6.bt2`, `NAME.rev.1.bt2`, `NAME.rev.2.bt2`, 
`NAME.rev.5.bt2`, and `NAME.rev.6.bt2` where `NAME` is `<bt2_base>`.

</td></tr></table>

### Options

<table><tr><td>

    -f

</td><td>

The reference input files (specified as `<reference_in>`) are FASTA files
(usually having extension `.fa`, `.mfa`, `.fna` or similar).

</td></tr><tr><td id="hisat-build-options-c">

    -c

</td><td>

The reference sequences are given on the command line.  I.e. `<reference_in>` is
a comma-separated list of sequences rather than a list of FASTA files.

</td></tr>
</td></tra><tr><td id="hisat-build-options-large-index">

[`--large-index`]: #hisat-build-options-large-index

    --large-index

</td><td>

Force `hisat-build` to build a [large index](#small-and-large-indexes), even if the reference is less
than ~ 4 billion nucleotides inlong.

</td></tr>
<tr><td id="hisat-build-options-a">

[`-a`/`--noauto`]: #hisat-build-options-a

    -a/--noauto

</td><td>

Disable the default behavior whereby `hisat-build` automatically selects
values for the [`--bmax`], [`--dcv`] and [`--packed`] parameters according to
available memory.  Instead, user may specify values for those parameters.  If
memory is exhausted during indexing, an error message will be printed; it is up
to the user to try new parameters.

</td></tr><tr><td id="hisat-build-options-p">

[`--packed`]: #hisat-build-options-p
[`-p`/`--packed`]: #hisat-build-options-p

    -p/--packed

</td><td>

Use a packed (2-bits-per-nucleotide) representation for DNA strings. This saves
memory but makes indexing 2-3 times slower.  Default: off. This is configured
automatically by default; use [`-a`/`--noauto`] to configure manually.

</td></tr><tr><td id="hisat-build-options-bmax">

[`--bmax`]: #hisat-build-options-bmax

    --bmax <int>

</td><td>

The maximum number of suffixes allowed in a block.  Allowing more suffixes per
block makes indexing faster, but increases peak memory usage.  Setting this
option overrides any previous setting for [`--bmax`], or [`--bmaxdivn`]. 
Default (in terms of the [`--bmaxdivn`] parameter) is [`--bmaxdivn`] 4.  This is
configured automatically by default; use [`-a`/`--noauto`] to configure manually.

</td></tr><tr><td id="hisat-build-options-bmaxdivn">

[`--bmaxdivn`]: #hisat-build-options-bmaxdivn

    --bmaxdivn <int>

</td><td>

The maximum number of suffixes allowed in a block, expressed as a fraction of
the length of the reference.  Setting this option overrides any previous setting
for [`--bmax`], or [`--bmaxdivn`].  Default: [`--bmaxdivn`] 4.  This is
configured automatically by default; use [`-a`/`--noauto`] to configure manually.

</td></tr><tr><td id="hisat-build-options-dcv">

[`--dcv`]: #hisat-build-options-dcv

    --dcv <int>

</td><td>

Use `<int>` as the period for the difference-cover sample.  A larger period
yields less memory overhead, but may make suffix sorting slower, especially if
repeats are present.  Must be a power of 2 no greater than 4096.  Default: 1024.
 This is configured automatically by default; use [`-a`/`--noauto`] to configure
manually.

</td></tr><tr><td id="hisat-build-options-nodc">

[`--nodc`]: #hisat-build-options-nodc

    --nodc

</td><td>

Disable use of the difference-cover sample.  Suffix sorting becomes
quadratic-time in the worst case (where the worst case is an extremely
repetitive reference).  Default: off.

</td></tr><tr><td>

    -r/--noref

</td><td>

Do not build the `NAME.3.bt2` and `NAME.4.bt2` portions of the index, which
contain a bitpacked version of the reference sequences and are used for
paired-end alignment.

</td></tr><tr><td>

    -3/--justref

</td><td>

Build only the `NAME.3.bt2` and `NAME.4.bt2` portions of the index, which
contain a bitpacked version of the reference sequences and are used for
paired-end alignment.

</td></tr><tr><td id="hisat-build-options-o">

    -o/--offrate <int>

</td><td>

To map alignments back to positions on the reference sequences, it's necessary
to annotate ("mark") some or all of the [Burrows-Wheeler] rows with their
corresponding location on the genome. 
[`-o`/`--offrate`](#hisat-build-options-o) governs how many rows get marked:
the indexer will mark every 2^`<int>` rows.  Marking more rows makes
reference-position lookups faster, but requires more memory to hold the
annotations at runtime.  The default is 4 (every 16th row is marked; for human
genome, annotations occupy about 680 megabytes).  

</td></tr><tr><td>

    -t/--ftabchars <int>

</td><td>

The ftab is the lookup table used to calculate an initial [Burrows-Wheeler]
range with respect to the first `<int>` characters of the query.  A larger
`<int>` yields a larger lookup table but faster query times.  The ftab has size
4^(`<int>`+1) bytes.  The default setting is 10 (ftab is 4MB).


</td></tr><tr><td id="hisat-build-options-localoffrate">

    --localoffrate <int>

</td><td>

This option governs how many rows get marked in a local index:
the indexer will mark every 2^`<int>` rows.  Marking more rows makes
reference-position lookups faster, but requires more memory to hold the
annotations at runtime.  The default is 3 (every 8th row is marked,
this occupies about 16KB per local index).  

</td></tr><tr><td>

    --localftabchars <int>

</td><td>

The local ftab is the lookup table in a local index.
The default setting is 6 (ftab is 8KB per local index).

</td></tr><tr><td>

    --seed <int>

</td><td>

Use `<int>` as the seed for pseudo-random number generator.

</td></tr><tr><td>

    --cutoff <int>

</td><td>

Index only the first `<int>` bases of the reference sequences (cumulative across
sequences) and ignore the rest.

</td></tr><tr><td>

    -q/--quiet

</td><td>

`hisat-build` is verbose by default.  With this option `hisat-build` will
print only error messages.

</td></tr><tr><td>

    -h/--help

</td><td>

Print usage information and quit.

</td></tr><tr><td>

    --version

</td><td>

Print version information and quit.

</td></tr></table>

The `hisat-inspect` index inspector
=====================================

`hisat-inspect` extracts information from a HISAT index about what kind of
index it is and what reference sequences were used to build it. When run without
any options, the tool will output a FASTA file containing the sequences of the
original references (with all non-`A`/`C`/`G`/`T` characters converted to `N`s).
 It can also be used to extract just the reference sequence names using the
[`-n`/`--names`] option or a more verbose summary using the [`-s`/`--summary`]
option.

Command Line
------------

Usage:

    hisat-inspect [options]* <bt2_base>

### Main arguments

<table><tr><td>

    <bt2_base>

</td><td>

The basename of the index to be inspected.  The basename is name of any of the
index files but with the `.X.bt2` or `.rev.X.bt2` suffix omitted.
`hisat-inspect` first looks in the current directory for the index files, then
in the directory specified in the `HISAT_INDEXES` environment variable.

</td></tr></table>

### Options

<table><tr><td>

    -a/--across <int>

</td><td>

When printing FASTA output, output a newline character every `<int>` bases
(default: 60).

</td></tr><tr><td id="hisat-inspect-options-n">

[`-n`/`--names`]: #hisat-inspect-options-n

    -n/--names

</td><td>

Print reference sequence names, one per line, and quit.

</td></tr><tr><td id="hisat-inspect-options-s">

[`-s`/`--summary`]: #hisat-inspect-options-s

    -s/--summary

</td><td>

Print a summary that includes information about index settings, as well as the
names and lengths of the input sequences.  The summary has this format: 

    Colorspace	<0 or 1>
    SA-Sample	1 in <sample>
    FTab-Chars	<chars>
    Sequence-1	<name>	<len>
    Sequence-2	<name>	<len>
    ...
    Sequence-N	<name>	<len>

Fields are separated by tabs.  Colorspace is always set to 0 for HISAT.

</td></tr><tr><td>

    -v/--verbose

</td><td>

Print verbose output (for debugging).

</td></tr><tr><td>

    --version

</td><td>

Print version information and quit.

</td></tr><tr><td>

    -h/--help

</td><td>

Print usage information and quit.

</td></tr></table>

Getting started with HISAT
===================================================

HISAT comes with some example files to get you started.  The example files
are not scientifically significant; these files will simply let you start running HISAT and
downstream tools right away.

First follow the manual instructions to [obtain HISAT].  Set the `HISAT_HOME`
environment variable to point to the new HISAT directory containing the
`hisat`, `hisat-build` and `hisat-inspect` binaries.  This is important,
as the `HISAT_HOME` variable is used in the commands below to refer to that
directory.

[obtain HISAT]: #obtaining-hisat

Indexing a reference genome
---------------------------

To create an index for the genomic region (1 million bps from the human chromosome 22 between 20,000,000 and 20,999,999)
included with HISAT, create a new temporary directory (it doesn't matter where), change into that directory, and run:

    $HISAT_HOME/hisat-build $HISAT_HOME/example/reference/22_20-21M.fa 22_20-21M_hisat

The command should print many lines of output then quit. When the command
completes, the current directory will contain ten new files that all start with
`22_20-21M_hisat` and end with `.1.bt2`, `.2.bt2`, `.3.bt2`, `.4.bt2`, `.5.bt2`, `.6.bt2`,
`.rev.1.bt2`, `.rev.2.bt2`, `.rev.5.bt2`, and `.rev.6.bt2`.  These files constitute the index - you're done!

You can use `hisat-build` to create an index for a set of FASTA files obtained
from any source, including sites such as [UCSC], [NCBI], and [Ensembl]. When
indexing multiple FASTA files, specify all the files using commas to separate
file names.  For more details on how to create an index with `hisat-build`,
see the [manual section on index building].  You may also want to bypass this
process by obtaining a pre-built index.

[UCSC]: http://genome.ucsc.edu/cgi-bin/hgGateway
[NCBI]: http://www.ncbi.nlm.nih.gov/sites/genome
[Ensembl]: http://www.ensembl.org/
[manual section on index building]: #the-hisat-build-indexer
[using a pre-built index]: #using-a-pre-built-index

Aligning example reads
----------------------

Stay in the directory created in the previous step, which now contains the
`22_20-21M_hisat` index files.  Next, run:

    $HISAT_HOME/hisat -x 22_20-21M_hisat -U $HISAT_HOME/example/reads/reads_1.fq -S eg1.sam

This runs the HISAT aligner, which aligns a set of unpaired reads to the
the genome region using the index generated in the previous step.
The alignment results in SAM format are written to the file `eg1.sam`, and a
short alignment summary is written to the console.  (Actually, the summary is
written to the "standard error" or "stderr" filehandle, which is typically
printed to the console.)

To see the first few lines of the SAM output, run:

    head eg1.sam

You will see something like this:

    @HD VN:1.0   SO:unsorted
    @SQ SN:22:20000000-20999999	LN:1000000
    @PG ID:hisat			PN:hisat	VN:0.1.0
    1   0				22:20000000-20999999	4115	255	100M			*	0	0	GGAGCGCAGCGTGGGCGGCCCCGCAGCGCGGCCTCGGACCCCAGAAGGGCTTCCCCGGGTCCGTTGGCGCGCGGGGAGCGGCGTTCCCAGGGCGCGGCGC IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0 XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    2   16				22:20000000-20999999	4197	255	100M			*	0	0	GTTCCCAGGGCGCGGCGCGGTGCGGCGCGGCGCGGGTCGCAGTCCACGCGGCCGCAACTCGGACCGGTGCGGGGGCCGCCCCCTCCCTCCAGGCCCAGCG IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    3   0				22:20000000-20999999	4113	255	100M			*	0	0	CTGGAGCGCAGCGTGGGCGGCCCCGCAGCGCGGCCTCGGACCCCAGAAGGGCTTCCCCGGGTCCGTTGGCGCGCGGGGAGCGGCGTTCCCAGGGCGCGGC IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    4   0				22:20000000-20999999	52358	255	100M			*	0	0	TTCAGGGTCTGCCTTTATGCCAGTGAGGAGCAGCAGAGTCTGATACTAGGTCTAGGACCGGCCGAGGTATACCATGAACATGTGGATACACCTGAGCCCA IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    5   16				22:20000000-20999999	52680	255	100M			*	0	0	CTTCTGGCCAGTAGGTCTTTGTTCTGGTCCAACGACAGGAGTAGGCTTGTATTTAAAAGCGGCCCCTCCTCTCCTGTGGCCACAGAACACAGGCGTGCTT IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    6   16				22:20000000-20999999	52664	255	100M			*	0	0	TCTCACCTCTCATGTGCTTCTGGCCAGTAGGTCTTTGTTCTGGTCCAACGACAGGAGTAGGCTTGTATTTAAAAGCGGCCCCTCCTCTCCTGTGGCCACA IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    7   0				22:20000000-20999999	52468	255	100M			*	0	0	TGTACACAGGCACTCACATGGCACACACATACACTCCTGCGTGTGCACAAGCACACACATGCAAGCCATATACATGGACACCGACACAGGCACATGTACG IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    8   0				22:20000000-20999999	4538	255	100M			*	0	0	CGGCCCCGCACCTGCCCGAACCTCTGCGGCGGCGGTGGCAGGGTACGCGGGACCGCTCCCTCCCAGCCGACTTACGAGAACATCCCCCGACCATCCAGCC IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:0 XN:i:0	XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU
    9   16				22:20000000-20999999	4667	255	50M19567N50M	*	0	0	CTTCCCCGGACTCTGGCCGCGTAGCCTCCGCCACCACTCCCAGTTCACAGACCTCGCGACCTGTGTCAGCAGAGCCGCCCTGCACCACCATGTGCATCAT IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:-1 XN:i:0 XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU XS:A:+
    10  0				22:20000000-20999999	30948	255	20M9021N80M		*	0	0	CAACAACGAGATCCTCAGTGGGCTGGACATGGAGGAAGGCAAGGAAGGAGGCACATGGCTGGGCATCAGCACACGTGGCAAGCTGGCAGCACTCACCAAC IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:-1 XN:i:0 XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU XS:A:+
    11  16				22:20000000-20999999	40044	255	65M8945N35M		*	0	0	TGGCAAGCTGGCAGCACTCACCAACTACCTGCAGCCGCAGCTGGACTGGCAGGCCCGAGGGCGAGGCACCTACGGGCTGAGCAACGCGCTGCTGGAGACT IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII AS:i:-1 XN:i:0 XM:i:0 XO:i:0 XG:i:0 NM:i:0 MD:Z:100 YT:Z:UU XS:A:+

The first few lines (beginning with `@`) are SAM header lines, and the rest of
the lines are SAM alignments, one line per read or mate.  See the [HISAT
manual section on SAM output] and the [SAM specification] for details about how
to interpret the SAM file format.

[HISAT manual section on SAM output]: #sam-output

Paired-end example
------------------

To align paired-end reads included with HISAT, stay in the same directory and
run:

    $HISAT_HOME/hisat -x 22_20-21M_hisat -1 $HISAT_HOME/example/reads/reads_1.fq -2 $HISAT_HOME/example/reads/reads_2.fq -S eg2.sam

This aligns a set of paired-end reads to the reference genome, with results
written to the file `eg2.sam`.

Using SAMtools/BCFtools downstream
----------------------------------

[SAMtools] is a collection of tools for manipulating and analyzing SAM and BAM
alignment files.  [BCFtools] is a collection of tools for calling variants and
manipulating VCF and BCF files, and it is typically distributed with [SAMtools].
Using these tools together allows you to get from alignments in SAM format to
variant calls in VCF format.  This example assumes that `samtools` and
`bcftools` are installed and that the directories containing these binaries are
in your [PATH environment variable].

Run the paired-end example:

    $HISAT_HOME/hisat -x $HISAT_HOME/example/index/22_20-21M_hisat -1 $HISAT_HOME/example/reads/reads_1.fq -2 $HISAT_HOME/example/reads/reads_2.fq -S eg2.sam

Use `samtools view` to convert the SAM file into a BAM file.  BAM is a the
binary format corresponding to the SAM text format.  Run:

    samtools view -bS eg2.sam > eg2.bam

Use `samtools sort` to convert the BAM file to a sorted BAM file.

    samtools sort eg2.bam eg2.sorted

We now have a sorted BAM file called `eg2.sorted.bam`. Sorted BAM is a useful
format because the alignments are (a) compressed, which is convenient for
long-term storage, and (b) sorted, which is conveneint for variant discovery.
To generate variant calls in VCF format, run:

    samtools mpileup -uf $HISAT_HOME/example/reference/22_20-21M.fa eg2.sorted.bam | bcftools view -bvcg - > eg2.raw.bcf

Then to view the variants, run:

    bcftools view eg2.raw.bcf

See the official SAMtools guide to [Calling SNPs/INDELs with SAMtools/BCFtools]
for more details and variations on this process.

[BCFtools]: http://samtools.sourceforge.net/mpileup.shtml
[Calling SNPs/INDELs with SAMtools/BCFtools]: http://samtools.sourceforge.net/mpileup.shtml
