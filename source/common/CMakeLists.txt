# vim: syntax=cmake

list(APPEND VFLAGS "-DX265_VERSION=${X265_VERSION}")
if(EXTRA_LIB)
    if(LINKED_8BIT)
        list(APPEND VFLAGS "-DLINKED_8BIT=1")
    endif(LINKED_8BIT)
    if(LINKED_10BIT)
        list(APPEND VFLAGS "-DLINKED_10BIT=1")
    endif(LINKED_10BIT)
    if(LINKED_12BIT)
        list(APPEND VFLAGS "-DLINKED_12BIT=1")
    endif(LINKED_12BIT)
endif(EXTRA_LIB)

if(ENABLE_ASSEMBLY)
    set_source_files_properties(threading.cpp primitives.cpp pixel.cpp PROPERTIES COMPILE_FLAGS -DENABLE_ASSEMBLY=1)
    list(APPEND VFLAGS "-DENABLE_ASSEMBLY=1")
endif(ENABLE_ASSEMBLY)

if(ENABLE_ASSEMBLY AND X86)
    set(SSE3  vec/dct-sse3.cpp)
    set(SSSE3 vec/dct-ssse3.cpp)
    set(SSE41 vec/dct-sse41.cpp)

    if(MSVC)
        set(PRIMITIVES ${SSE3} ${SSSE3} ${SSE41})
        set(WARNDISABLE "/wd4100") # unreferenced formal parameter
        if(INTEL_CXX)
            add_definitions(/Qwd111) # statement is unreachable
            add_definitions(/Qwd128) # loop is unreachable
            add_definitions(/Qwd177) # declared function is unused
            add_definitions(/Qwd185) # dynamic initialization in unreachable code
            add_definitions(/Qwd280) # conditional expression is constant
        endif()
        if(X64)
            set_source_files_properties(${SSE3} ${SSSE3} ${SSE41} PROPERTIES COMPILE_FLAGS "${WARNDISABLE}")
        else()
            # x64 implies SSE4, so only add /arch:SSE2 if building for Win32
            set_source_files_properties(${SSE3} ${SSSE3} ${SSE41} PROPERTIES COMPILE_FLAGS "${WARNDISABLE} /arch:SSE2")
        endif()
    endif()
    if(GCC)
        if(CLANG)
            # llvm intrinsic headers cause shadow warnings
            set(WARNDISABLE "-Wno-shadow -Wno-unused-parameter")
        else()
            set(WARNDISABLE "-Wno-unused-parameter")
        endif()
        if(INTEL_CXX OR CLANG OR (NOT CC_VERSION VERSION_LESS 4.3))
            set(PRIMITIVES ${SSE3} ${SSSE3} ${SSE41})
            set_source_files_properties(${SSE3}  PROPERTIES COMPILE_FLAGS "${WARNDISABLE} -msse3")
            set_source_files_properties(${SSSE3} PROPERTIES COMPILE_FLAGS "${WARNDISABLE} -mssse3")
            set_source_files_properties(${SSE41} PROPERTIES COMPILE_FLAGS "${WARNDISABLE} -msse4.1")
        endif()
    endif()
    set(VEC_PRIMITIVES vec/vec-primitives.cpp ${PRIMITIVES})
    source_group(Intrinsics FILES ${VEC_PRIMITIVES})
    set(C_SRCS asm-primitives.cpp pixel.h mc.h ipfilter8.h blockcopy8.h dct8.h loopfilter.h seaintegral.h)
    set(A_SRCS pixel-a.asm const-a.asm cpu-a.asm ssd-a.asm mc-a.asm
               mc-a2.asm pixel-util8.asm blockcopy8.asm
               pixeladd8.asm dct8.asm seaintegral.asm)
    if(HIGH_BIT_DEPTH)
        set(A_SRCS ${A_SRCS} sad16-a.asm intrapred16.asm v4-ipfilter16.asm h4-ipfilter16.asm h-ipfilter16.asm ipfilter16.asm loopfilter.asm)
    else()
        set(A_SRCS ${A_SRCS} sad-a.asm intrapred8.asm intrapred8_allangs.asm v4-ipfilter8.asm h-ipfilter8.asm ipfilter8.asm loopfilter.asm)
    endif()
    if(NOT X64)
        set(A_SRCS ${A_SRCS} pixel-32.asm)
    endif()

    if(MSVC_IDE OR XCODE)
        # MSVC requires custom build rules in the main cmake script for nasm
        set(MSVC_ASMS "${A_SRCS}" CACHE INTERNAL "nasm sources")
        set(A_SRCS)
    endif()

    enable_language(ASM_NASM)

    foreach(SRC ${A_SRCS} ${C_SRCS})
        set(ASM_PRIMITIVES ${ASM_PRIMITIVES} x86/${SRC})
    endforeach()
    source_group(Assembly FILES ${ASM_PRIMITIVES})
endif(ENABLE_ASSEMBLY AND X86)

if(ENABLE_ASSEMBLY AND (ARM OR CROSS_COMPILE_ARM))
    set(C_SRCS asm-primitives.cpp pixel.h mc.h ipfilter8.h blockcopy8.h dct8.h loopfilter.h)

    # add ARM assembly/intrinsic files here
    set(A_SRCS asm.S cpu-a.S mc-a.S sad-a.S pixel-util.S ssd-a.S blockcopy8.S ipfilter8.S dct-a.S)
    set(VEC_PRIMITIVES)

    set(ARM_ASMS "${A_SRCS}" CACHE INTERNAL "ARM Assembly Sources")
    foreach(SRC ${C_SRCS})
        set(ASM_PRIMITIVES ${ASM_PRIMITIVES} arm/${SRC})
    endforeach()
    source_group(Assembly FILES ${ASM_PRIMITIVES})
endif(ENABLE_ASSEMBLY AND (ARM OR CROSS_COMPILE_ARM))

if(ENABLE_ASSEMBLY AND (ARM64 OR CROSS_COMPILE_ARM64))
    if(GCC AND (CMAKE_CXX_FLAGS_RELEASE MATCHES "-O3"))
        message(STATUS "Detected CXX compiler using -O3 optimization level")
        add_definitions(-DAUTO_VECTORIZE=1)
    endif()

    # Add Arm intrinsics files here.
    set(C_SRCS_NEON asm-primitives.cpp pixel-prim.h pixel-prim.cpp filter-prim.h filter-prim.cpp dct-prim.h dct-prim.cpp loopfilter-prim.cpp loopfilter-prim.h intrapred-prim.cpp arm64-utils.cpp arm64-utils.h fun-decls.h sao-prim.cpp  mem-neon.h)
    set(C_SRCS_NEON_DOTPROD filter-neon-dotprod.cpp pixel-prim-neon-dotprod.cpp)
    set(C_SRCS_NEON_I8MM filter-neon-i8mm.cpp)
    set(C_SRCS_SVE sao-prim-sve.cpp dct-prim-sve.cpp filter-prim-sve.h filter-prim-sve.cpp pixel-prim-sve.cpp)
    set(C_SRCS_SVE2 sao-prim-sve2.cpp)
    enable_language(ASM)

    # Add Arm assembly files here.
    set(A_SRCS asm.S mc-a.S mc-a-common.S sad-a.S pixel-util.S pixel-util-common.S p2s.S p2s-common.S blockcopy8.S ssd-a.S ssd-a-common.S intrapred.S dct.S)
    set(A_SRCS_NEON_DOTPROD sad-neon-dotprod.S ssd-neon-dotprod.S)
    set(A_SRCS_SVE asm-sve.S p2s-sve.S pixel-util-sve.S ssd-a-sve.S)
    set(A_SRCS_SVE2 mc-a-sve2.S pixel-util-sve2.S)
    set(A_SRCS_SVE2_BITPERM pixel-util-sve2-bitperm.S)
    set(VEC_PRIMITIVES)

    set(ARM_ASMS "${A_SRCS}" CACHE INTERNAL "Arm Assembly Sources")
    set(ARM_ASMS_NEON_DOTPROD "${A_SRCS_NEON_DOTPROD}" CACHE INTERNAL "Arm Assembly Sources that use the Neon DotProd extension")
    set(ARM_ASMS_SVE "${A_SRCS_SVE}" CACHE INTERNAL "Arm Assembly Sources that use the SVE extension")
    set(ARM_ASMS_SVE2 "${A_SRCS_SVE2}" CACHE INTERNAL "Arm Assembly Sources that use the SVE2 extension")
    set(ARM_ASMS_SVE2_BITPERM "${A_SRCS_SVE2_BITPERM}" CACHE INTERNAL "Arm Assembly Sources that use the SVE2 BitPerm extension")
    foreach(SRC ${C_SRCS_NEON})
        set(ASM_PRIMITIVES ${ASM_PRIMITIVES} aarch64/${SRC})
        set_source_files_properties(aarch64/${SRC} PROPERTIES COMPILE_FLAGS ${AARCH64_NEON_FLAG})
    endforeach()

    if(CPU_HAS_NEON_DOTPROD)
        foreach(SRC ${C_SRCS_NEON_DOTPROD})
            set(ASM_PRIMITIVES ${ASM_PRIMITIVES} aarch64/${SRC})
            set_source_files_properties(aarch64/${SRC} PROPERTIES COMPILE_FLAGS ${AARCH64_NEON_DOTPROD_FLAG})
        endforeach()
    endif()

    if(CPU_HAS_NEON_I8MM)
        foreach(SRC ${C_SRCS_NEON_I8MM})
            set(ASM_PRIMITIVES ${ASM_PRIMITIVES} aarch64/${SRC})
            set_source_files_properties(aarch64/${SRC} PROPERTIES COMPILE_FLAGS ${AARCH64_NEON_I8MM_FLAG})
        endforeach()
    endif()

    if(CPU_HAS_SVE AND HAVE_SVE_BRIDGE)
        foreach(SRC ${C_SRCS_SVE})
            set(ASM_PRIMITIVES ${ASM_PRIMITIVES} aarch64/${SRC})
            set_source_files_properties(aarch64/${SRC} PROPERTIES COMPILE_FLAGS ${AARCH64_SVE_FLAG})
        endforeach()
    endif()

    if(CPU_HAS_SVE2 AND HAVE_SVE_BRIDGE)
        foreach(SRC ${C_SRCS_SVE2})
            set(ASM_PRIMITIVES ${ASM_PRIMITIVES} aarch64/${SRC})
            set_source_files_properties(aarch64/${SRC} PROPERTIES COMPILE_FLAGS ${AARCH64_SVE2_FLAG})
        endforeach()
    endif()

    source_group(Assembly FILES ${ASM_PRIMITIVES})

    if(AARCH64_WARNINGS_AS_ERRORS)
        set_source_files_properties(${ASM_PRIMITIVES} PROPERTIES COMPILE_FLAGS -Werror)
    endif()
endif(ENABLE_ASSEMBLY AND (ARM64 OR CROSS_COMPILE_ARM64))

if(POWER)
    set_source_files_properties(version.cpp PROPERTIES COMPILE_FLAGS -DX265_VERSION=${X265_VERSION})
    if(ENABLE_ALTIVEC)
        set(ALTIVEC_SRCS pixel_altivec.cpp dct_altivec.cpp ipfilter_altivec.cpp intrapred_altivec.cpp)
        foreach(SRC ${ALTIVEC_SRCS})
            set(ALTIVEC_PRIMITIVES ${ALTIVEC_PRIMITIVES} ppc/${SRC})
        endforeach()
        source_group(Intrinsics_altivec FILES ${ALTIVEC_PRIMITIVES})
        set_source_files_properties(${ALTIVEC_PRIMITIVES} PROPERTIES COMPILE_FLAGS "-Wno-unused  -Wno-unknown-pragmas -Wno-maybe-uninitialized")
    endif()
endif()


# set_target_properties can't do list expansion
string(REPLACE ";" " " VERSION_FLAGS "${VFLAGS}")
set_source_files_properties(version.cpp PROPERTIES COMPILE_FLAGS ${VERSION_FLAGS})

check_symbol_exists(strtok_r "string.h" HAVE_STRTOK_R)
if(HAVE_STRTOK_R)
    set_source_files_properties(param.cpp PROPERTIES COMPILE_FLAGS -DHAVE_STRTOK_R=1)
endif()

if(GCC AND CC_HAS_NO_NARROWING)
    set_source_files_properties(cpu.cpp PROPERTIES COMPILE_FLAGS -Wno-narrowing)
endif()
if(WIN32)
    set(WINXP winxp.h winxp.cpp)
endif(WIN32)

add_library(common OBJECT
    ${ASM_PRIMITIVES} ${VEC_PRIMITIVES} ${ALTIVEC_PRIMITIVES} ${WINXP}
    primitives.cpp primitives.h
    pixel.cpp dct.cpp lowpassdct.cpp ipfilter.cpp intrapred.cpp loopfilter.cpp
    constants.cpp constants.h
    cpu.cpp cpu.h version.cpp
    threading.cpp threading.h
    threadpool.cpp threadpool.h
    wavefront.h wavefront.cpp
    md5.cpp md5.h
    bitstream.h bitstream.cpp
    yuv.cpp yuv.h
    shortyuv.cpp shortyuv.h
    picyuv.cpp picyuv.h
    common.cpp common.h
    param.cpp param.h
    frame.cpp frame.h
    framedata.cpp framedata.h
    cudata.cpp cudata.h
    slice.cpp slice.h
    lowres.cpp lowres.h mv.h 
    piclist.cpp piclist.h
    predict.cpp  predict.h
    scalinglist.cpp scalinglist.h
    quant.cpp quant.h contexts.h
    deblock.cpp deblock.h
    scaler.cpp scaler.h
    ringmem.cpp ringmem.h
    temporalfilter.cpp temporalfilter.h)
