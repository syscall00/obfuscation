set(LLVM_LINK_COMPONENTS
  Obfuscation
  TransformUtils
  ${LLVM_TARGETS_TO_BUILD}
  BitReader
  BitWriter
  Core
  IRReader
  LTO
  MC
  Object
  Support
  Target
  )

add_llvm_tool(llvm-lto
  llvm-lto.cpp

  DEPENDS
  intrinsics_gen
  )

