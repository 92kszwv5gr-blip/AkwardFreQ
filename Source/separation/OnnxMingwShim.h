#pragma once

// ONNX Runtime's C API headers assume an MSVC-flavoured Windows toolchain:
// ORT_API_CALL expands to the single-underscore `_stdcall` (MSVC spelling,
// unrecognised by GCC/mingw, which only knows `__stdcall`), and the header
// pulls in SAL annotation macros (_Frees_ptr_opt_ etc.) via <specstrings.h>
// that mingw-w64's own sal.h does not fully define. Neither issue affects a
// real MSVC build, so this is scoped to mingw only.
#if defined(__MINGW32__) || defined(__MINGW64__)
#define _stdcall __stdcall

#undef _In_
#undef _In_z_
#undef _In_opt_
#undef _In_opt_z_
#undef _Out_
#undef _Outptr_
#undef _Out_opt_
#undef _Inout_
#undef _Inout_opt_
#undef _Frees_ptr_opt_
#undef _Ret_maybenull_
#undef _Ret_notnull_
#undef _Check_return_
#undef _Outptr_result_maybenull_
#undef _In_reads_
#undef _Inout_updates_
#undef _Out_writes_
#undef _Inout_updates_all_
#undef _Out_writes_bytes_all_
#undef _Out_writes_all_
#undef _Success_
#undef _Outptr_result_buffer_maybenull_

#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _Out_
#define _Outptr_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _Frees_ptr_opt_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Check_return_
#define _Outptr_result_maybenull_
#define _In_reads_(X)
#define _Inout_updates_(X)
#define _Out_writes_(X)
#define _Inout_updates_all_(X)
#define _Out_writes_bytes_all_(X)
#define _Out_writes_all_(X)
#define _Success_(X)
#define _Outptr_result_buffer_maybenull_(X)
#endif
