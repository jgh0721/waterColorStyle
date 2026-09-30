# 이 저장소의 대상(라이브러리 · 앱 · 플러그인)이 같이 쓰는 컴파일 옵션.
#   MSVC: 한국어 문자열 리터럴은 UTF-8이므로 /utf-8. /w14062 = switch에서 빠진 enum 값 경고(C4062) —
#   Variant에 Navy를 더할 때처럼 enum이 늘어나도 조용히 기본 분기로 가지 않게 한다.
function(fm_set_compile_options target)
    target_compile_options(${target}
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /Zc:__cplusplus /w14062>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wswitch-enum>
    )
endfunction()
