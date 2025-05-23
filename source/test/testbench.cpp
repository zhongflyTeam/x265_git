/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Gopu Govindaswamy <gopu@govindaswamy.org>
 *          Mandar Gurav <mandar@multicorewareinc.com>
 *          Mahesh Pittala <mahesh@multicorewareinc.com>
 *          Min Chen <chenm003@163.com>
 *          Yimeng Su <yimeng.su@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#include "common.h"
#include "primitives.h"
#include "pixelharness.h"
#include "mbdstharness.h"
#include "ipfilterharness.h"
#include "intrapredharness.h"
#include "param.h"
#include "cpu.h"

using namespace X265_NS;

const char* lumaPartStr[NUM_PU_SIZES] =
{
    "  4x4", "  8x8", "16x16", "32x32", "64x64",
    "  8x4", "  4x8",
    " 16x8", " 8x16",
    "32x16", "16x32",
    "64x32", "32x64",
    "16x12", "12x16", " 16x4", " 4x16",
    "32x24", "24x32", " 32x8", " 8x32",
    "64x48", "48x64", "64x16", "16x64",
};

const char* chromaPartStr420[NUM_PU_SIZES] =
{
    "  2x2", "  4x4", "  8x8", "16x16", "32x32",
    "  4x2", "  2x4",
    "  8x4", "  4x8",
    " 16x8", " 8x16",
    "32x16", "16x32",
    "  8x6", "  6x8", "  8x2", "  2x8",
    "16x12", "12x16", " 16x4", " 4x16",
    "32x24", "24x32", " 32x8", " 8x32",
};

const char* chromaPartStr422[NUM_PU_SIZES] =
{
    "  2x4", "  4x8", " 8x16", "16x32", "32x64",
    "  4x4", "  2x8",
    "  8x8", " 4x16",
    "16x16", " 8x32",
    "32x32", "16x64",
    " 8x12", " 6x16", "  8x4", " 2x16",
    "16x24", "12x32", " 16x8", " 4x32",
    "32x48", "24x64", "32x16", " 8x64",
};

const char* const* chromaPartStr[X265_CSP_COUNT] =
{
    lumaPartStr,
    chromaPartStr420,
    chromaPartStr422,
    lumaPartStr
};

struct test_arch_t
{
    char name[13];
    int flag;
} testArch[] =
{
#if X265_ARCH_X86
    { "SSE2", X265_CPU_SSE2 },
    { "SSE3", X265_CPU_SSE3 },
    { "SSSE3", X265_CPU_SSSE3 },
    { "SSE4", X265_CPU_SSE4 },
    { "AVX", X265_CPU_AVX },
    { "XOP", X265_CPU_XOP },
    { "AVX2", X265_CPU_AVX2 },
    { "BMI2", X265_CPU_AVX2 | X265_CPU_BMI1 | X265_CPU_BMI2 },
    { "AVX512", X265_CPU_AVX512 },
#else
    { "ARMv6", X265_CPU_ARMV6 },
    { "NEON", X265_CPU_NEON },
    { "Neon_DotProd", X265_CPU_NEON_DOTPROD },
    { "Neon_I8MM", X265_CPU_NEON_I8MM },
    { "SVE", X265_CPU_SVE },
    { "SVE2", X265_CPU_SVE2 },
    { "SVE2_BitPerm", X265_CPU_SVE2_BITPERM },
    { "FastNeonMRC", X265_CPU_FAST_NEON_MRC },
#endif
    { "", 0 },
};

void do_cpuid_list(int cpuid)
{
    printf("x265 detected --cpuid architectures:\n");
    for (int i = 0; testArch[i].flag; i++)
    {
        if ((testArch[i].flag & cpuid) == testArch[i].flag)
            printf("       %s\n", testArch[i].name);
    }
}

void do_help()
{
    printf("x265 optimized primitive testbench\n\n");
    printf("usage: TestBench [--cpuid CPU] [--testbench BENCH] [--nobench] [--help]\n\n");
    printf("       CPU is comma separated SIMD architecture list, for example: SSE4,AVX\n");
    printf("       Use `--cpuid list` to print a list of detected SIMD architectures\n\n");
    printf("       BENCH is one of (pixel,transforms,interp,intrapred)\n\n");
    printf("       `--nobench` disables running benchmarks, only run correctness tests\n\n");
    printf("By default, the test bench will test all benches on detected CPU architectures\n");
    printf("Options and testbench name may be truncated.\n");
}

PixelHarness  HPixel;
MBDstHarness  HMBDist;
IPFilterHarness HIPFilter;
IntraPredHarness HIPred;

int main(int argc, char *argv[])
{
    bool enableavx512 = true;
    int cpuid = X265_NS::cpu_detect(enableavx512);
    const char *testname = 0;
    bool run_benchmarks = true;

    for (int i = 1; i < argc; )
    {
        if (strncmp(argv[i], "--", 2))
        {
            printf("** invalid long argument: %s\n\n", argv[i]);
            do_help();
            return 1;
        }
        const char *name = argv[i] + 2;
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        if (!strncmp(name, "help", strlen(name)))
        {
          do_help();
          return 0;
        }
        else if (!strncmp(name, "cpuid", strlen(name)))
        {
            if (!strncmp(value, "list", 5))
            {
                do_cpuid_list(cpuid);
                return 0;
            }
            int cpu_detect_cpuid = cpuid;
            bool bError = false;
            cpuid = parseCpuName(value, bError, enableavx512);
            if (bError)
            {
                printf("Invalid CPU name: %s\n", value);
                return 1;
            }
            else if ((cpuid & cpu_detect_cpuid) != cpuid)
            {
                printf("Feature detection conflicts with provided --cpuid: %s\n", value);
                return 1;
            }
            i += 2;
        }
        else if (!strncmp(name, "testbench", strlen(name)))
        {
            testname = value;
            printf("Testing only harnesses that match name <%s>\n", testname);
            i += 2;
        }
        else if (!strncmp(name, "nobench", strlen(name)))
        {
            printf("Disabling performance benchmarking\n");
            run_benchmarks = false;
            i += 1;
        }
        else
        {
            printf("** invalid long argument: %s\n\n", name);
            do_help();
            return 1;
        }
    }

    int seed = (int)time(NULL);
    printf("Using random seed %X %dbit\n", seed, X265_DEPTH);
    srand(seed);

    // To disable classes of tests, simply comment them out in this list
    TestHarness *harness[] =
    {
        &HPixel,
        &HMBDist,
        &HIPFilter,
        &HIPred
    };

    EncoderPrimitives cprim;
    memset(&cprim, 0, sizeof(EncoderPrimitives));
    setupCPrimitives(cprim);
    setupAliasPrimitives(cprim);

    for (int i = 0; testArch[i].flag; i++)
    {
        if ((testArch[i].flag & cpuid) != testArch[i].flag)
            continue;

        printf("Testing primitives: %s\n", testArch[i].name);
        fflush(stdout);

#if defined(X265_ARCH_X86) || defined(X265_ARCH_ARM64)
        EncoderPrimitives vecprim;
        memset(&vecprim, 0, sizeof(vecprim));
        setupIntrinsicPrimitives(vecprim, testArch[i].flag);
        setupAliasPrimitives(vecprim);
        for (size_t h = 0; h < sizeof(harness) / sizeof(TestHarness*); h++)
        {
            if (testname && strncmp(testname, harness[h]->getName(), strlen(testname)))
                continue;
            if (!harness[h]->testCorrectness(cprim, vecprim))
            {
                fflush(stdout);
                fprintf(stderr, "\nx265: intrinsic primitive has failed. Go and fix that Right Now!\n");
                return -1;
            }
        }
#endif

        EncoderPrimitives asmprim;
        memset(&asmprim, 0, sizeof(asmprim));

        setupAssemblyPrimitives(asmprim, testArch[i].flag);
        setupAliasPrimitives(asmprim);
        memcpy(&primitives, &asmprim, sizeof(EncoderPrimitives));
        for (size_t h = 0; h < sizeof(harness) / sizeof(TestHarness*); h++)
        {
            if (testname && strncmp(testname, harness[h]->getName(), strlen(testname)))
                continue;
            if (!harness[h]->testCorrectness(cprim, asmprim))
            {
                fflush(stdout);
                fprintf(stderr, "\nx265: asm primitive has failed. Go and fix that Right Now!\n");
                return -1;
            }
        }
    }

    /******************* Cycle count for all primitives **********************/
    if (run_benchmarks)
    {
        EncoderPrimitives optprim;
        memset(&optprim, 0, sizeof(optprim));
#if defined(X265_ARCH_X86) || defined(X265_ARCH_ARM64)
        setupIntrinsicPrimitives(optprim, cpuid);
#endif

        setupAssemblyPrimitives(optprim, cpuid);

        /* Note that we do not setup aliases for performance tests, that would be
         * redundant. The testbench only verifies they are correctly aliased */

        /* some hybrid primitives may rely on other primitives in the
         * global primitive table, so set up those pointers. This is a
         * bit ugly, but I don't see a better solution */
        memcpy(&primitives, &optprim, sizeof(EncoderPrimitives));

        printf("\nTest performance improvement with full optimizations\n");
        fflush(stdout);

        for (size_t h = 0; h < sizeof(harness) / sizeof(TestHarness*); h++)
        {
            if (testname && strncmp(testname, harness[h]->getName(), strlen(testname)))
                continue;
            printf("== %s primitives ==\n", harness[h]->getName());
            harness[h]->measureSpeed(cprim, optprim);
        }

        printf("\n");
    }
    return 0;
}
