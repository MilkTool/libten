# Ten 0.6 Reference Manual
The reference manual is the official definition of the Ten programming
language, it's based on the [Lua Reference Manual][lua-manual].

## Contents

* [1 - Introduction][ch1]
* [2 - Basic Concepts][ch2]
    *  [2.1 - Values][ch2.1]
    *  [2.2 - Records][ch2.2]
    *  [2.3 - Fibers][ch2.3]
    *  [2.4 - Undefined][ch2.4]
    *  [2.5 - Errors][ch2.5]
* [3 - The Language][ch3]
    *  [3.1 - Tokens][ch3.1]
    *  [3.2 - Sequences][ch3.2]
    *  [3.3 - Variables][ch3.3]
    *  [3.4 - Blocks][ch3.4]
    *  [3.5 - Conditionals][ch3.5]
    *  [3.6 - Signals][ch3.6]
    *  [3.7 - Constructors][ch3.7]
        * [3.7.1 - Closures][ch3.7.1]
        * [3.7.2 - Records][ch3.7.2]
        * [3.7.3 - Tuples][ch3.7.3]
    * [3.8 - Calls][ch3.8]
        * [3.8.1 - Recursion][ch3.8.1]
    * [3.9 - Arithmetic][ch3.9]
    * [3.10 - Logic][ch3.10]
    * [3.11 - Shifts][ch3.11]
    * [3.12 - Comparisons][ch3.12]
    * [3.13 - Replacement][ch3.13]
    * [3.14 - Assertion][ch3.14]
    * [3.15 - Fix][ch3.15]
    * [3.16 - Scripts][ch3.16]
* [4 - The Prelude][ch4]
    * [4.1 - Type Checking][ch4.1]
    * [4.2 - Type Conversion][ch4.2]
    * [4.3 - Number Parsing][ch4.3]
    * [4.4 - Iteration][ch4.4]
    * [4.5 - Strings and Text][ch4.5]
    * [4.6 - Linked Lists][ch4.6]
    * [4.7 - User IO][ch4.7]
    * [4.8 - Fibers][ch4.8]
    * [4.9 - Records][ch4.9]
    * [4.10 - Compiling][ch4.10]
    * [4.11 - Modules][ch4.11]
    * [4.12 - Pipelining][ch4.12]
    * [4.13 - Misc][ch4.13]
* [5 - The API][ch5]
    * [5.1 - Ten State][ch5.1]
    * [5.2 - Variables][ch5.2]
        * [5.2.1 - Stacked Variables][ch5.2.1]
        * [5.2.2 - Temporary Variables][ch5.2.2]
        * [5.2.3 - Accessing Values][ch5.2.3]
        * [5.2.4 - Changing Values][ch5.2.4]
    * [5.3 - Executing Code][ch5.3]
    * [5.4 - Compiling Code][ch5.4]
    * [5.5 - Accessing Globals][ch5.5]
    * [5.6 - Accessing Fields][ch5.6]
    * [5.7 - Type Checking][ch5.7]
    * [5.8 - Native Data][ch5.8]
        * [5.8.1 - Pointers][ch5.8.1]
        * [5.8.2 - Data Objects][ch5.8.2]
    * [5.9 - Native Closures][ch5.9]
        * [5.9.1 - Re-Entry][ch5.9.1]
    * [5.10 - Records][ch5.10]
    * [5.11 - Fibers][ch5.11]
    * [5.12 - Handling Errors][ch5.12]
    * [5.13 - Types and Functions][ch5.13]
* [6 - Ten Module Loader][ch6]
    * [6.1 - Project Modules][ch6.1]
    * [6.2 - Library Modules][ch6.2]
    * [6.3 - Script Modules][ch6.3]
    * [6.4 - Native Modules][ch6.4]
    * [6.5 - Data Modules][ch6.5]
* [7 - Ten CLI][ch7]
* [8 - Appendices][ch8]
    * [8.1 - Operators][ch8.1]
    * [8.2 - Cheatsheet][ch8.2]
    * [8.3 - EBNF Syntax][ch8.3]

[lua-manual]: https://www.lua.org/manual/
[ch1]:        introduction.md
[ch2]:        basic-concepts.md
[ch2.1]:      basic-concepts.md#2.1
[ch2.2]:      basic-concepts.md#2.2
[ch2.3]:      basic-concepts.md#2.3
[ch2.4]:      basic-concepts.md#2.4
[ch2.5]:      basic-concepts.md#2.5
[ch3]:        the-language.md
[ch3.1]:      the-language.md#3.1
[ch3.2]:      the-language.md#3.2
[ch3.3]:      the-language.md#3.3
[ch3.4]:      the-language.md#3.4
[ch3.5]:      the-language.md#3.5
[ch3.6]:      the-language.md#3.6
[ch3.7]:      the-language.md#3.7
[ch3.7.1]:    the-language.md#3.7.1
[ch3.7.2]:    the-language.md#3.7.2
[ch3.7.3]:    the-language.md#3.7.3
[ch3.8]:      the-language.md#3.8
[ch3.8.1]:    the-language.md#3.8.1
[ch3.9]:      the-language.md#3.9
[ch3.10]:     the-language.md#3.10
[ch3.11]:     the-language.md#3.11
[ch3.12]:     the-language.md#3.12
[ch3.13]:     the-language.md#3.13
[ch3.14]:     the-language.md#3.14
[ch3.15]:     the-language.md#3.15
[ch3.16]:     the-language.md#3.15
[ch4]:        the-prelude.md
[ch4.1]:      the-prelude.md#4.1
[ch4.2]:      the-prelude.md#4.2
[ch4.3]:      the-prelude.md#4.3
[ch4.4]:      the-prelude.md#4.4
[ch4.5]:      the-prelude.md#4.5
[ch4.6]:      the-prelude.md#4.6
[ch4.7]:      the-prelude.md#4.7
[ch4.8]:      the-prelude.md#4.8
[ch4.9]:      the-prelude.md#4.9
[ch4.10]:     the-prelude.md#4.10
[ch4.11]:     the-prelude.md#4.11
[ch4.12]:     the-prelude.md#4.12
[ch4.13]:     the-prelude.md#4.13
[ch5]:        the-api.md
[ch5.1]:      the-api.md#5.1
[ch5.2]:      the-api.md#5.2
[ch5.2.1]:    the-api.md#5.2.1
[ch5.2.2]:    the-api.md#5.2.2
[ch5.2.3]:    the-api.md#5.2.3
[ch5.2.4]:    the-api.md#5.2.4
[ch5.2.5]:    the-api.md#5.2.5
[ch5.2.6]:    the-api.md#5.2.6
[ch5.3]:      the-api.md#5.3
[ch5.4]:      the-api.md#5.4
[ch5.5]:      the-api.md#5.5
[ch5.6]:      the-api.md#5.6
[ch5.7]:      the-api.md#5.7
[ch5.8]:      the-api.md#5.8
[ch5.8.1]:    the-api.md#5.8.1
[ch5.8.2]:    the-api.md#5.8.2
[ch5.9]:      teh-api.md#5.9
[ch5.9.1]:    the-api.md#5.9.1
[ch5.9.2]:    the-api.md#5.9.2
[ch5.10]:     the-api.md#5.10
[ch5.11]:     the-api.md#5.11
[ch5.12]:     the-api.md#5.12
[ch5.13]:     the-api.md#5.13
[ch6]:        ten-module-loader.md
[ch6.1]:      ten-module-loader.md#6.1
[ch6.2]:      ten-module-loader.md#6.2
[ch6.3]:      ten-module-loader.md#6.3
[ch6.4]:      ten-module-loader.md#6.3
[ch6.5]:      ten-module-loader.md#6.3
[ch7]:        ten-cli.md
[ch8]:        appendices.md
[ch8.1]:      appendices.md#8.1
[ch8.2]:      appendices.md#8.2
[ch8.3]:      appendices.md#8.3


## Prelude Index

- [`require( mid )`][p-require]
- [`import( mid )`][p-import]
- [`loader( type, loadr ? trans )`][p-loader]
- [`collect()`][p-collect]
- [`type( val )`][p-type]
- [`expect( what, type, val )`][p-expect]
- [`assert()`][p-assert]
- [`clock()`][p-clock]
- [`rand()`][p-rand]
- [`log( val )`][p-log]
- [`int( val )`][p-int]
- [`dec( val )`][p-dec]
- [`sym( val )`][p-sym]
- [`str( val )`][p-str]
- [`hex( str )`][p-hex]
- [`oct( str )`][p-oct]
- [`bin( str )`][p-bin]
- [`keys( rec )`][p-keys]
- [`vals( rec )`][p-vals]
- [`pairs( rec )`][p-pairs]
- [`seq( vals... )`][p-seq]
- [`rseq( vals... )`][p-rseq]
- [`bytes( str )`][p-bytes]
- [`chars( str )`][p-chars]
- [`split( str, sep )`][p-split]
- [`items( list )`][p-items]
- [`drange( start, end ? step )`][p-drange]
- [`irange( start, end ? step )`][p-irange]
- [`show( vals... )`][p-show]
- [`warn( vals... )`][p-warn]
- [`input()`][p-input]
- [`ucode( chr )`][p-ucode]
- [`uchar( int )`][p-uchar]
- [`cat( vals... )`][p-cat]
- [`join( iter, sep )`][p-join]
- [`bcmp( str1, opr, str2 )`][p-bcmp]
- [`ccmp( str1, opr, str2 )`][p-ccmp]
- [`bsub( str, n )`][p-bsub]
- [`csub( str, n )`][p-csub]
- [`blen( str )`][p-blen]
- [`clen( str )`][p-clen]
- [`each( iter, what )`][p-each]
- [`fold( iter, agr, how)`][p-fold]
- [`sep( rec )`][p-sep]
- [`cons( car, cdr )`][p-cons]
- [`list( vals... )`][p-list]
- [`explode( iter )`][p-explode]
- [`fiber( cls ? tag )`][p-fiber]
- [`cont( fib, { args... } )`][p-cont]
- [`yield( vals... )`][p-yield]
- [`panic( err )`][p-panic]
- [`state( fib )`][p-state]
- [`errval( fib )`][p-errval]
- [`trace( fib )`][p-trace]
- [`script( upvals, script )`][p-script]
- [`expr( upvals, expr )`][p-expr]
- [`pipe( val, pipeline... )`][p-pipe]
- [`rpipe( val, { pipeline... } )`][p-rpipe]
- [`pump( iter, pipeline... )`][p-pump]
- [`rpump( iter, { pipeline... } )`][p-rpump]
- [`skip( iter, num )`][p-skip]
- [`limit( iter, lim )`][p-limit]
- [`N`][p-N]
- [`R`][p-R]
- [`L`][p-L]
- [`T`][p-T]

[p-require]:  the-prelude.md#fun-require
[p-import]:   the-prelude.md#fun-import
[p-loader]:   the-prelude.md#fun-loader
[p-collect]:  the-prelude.md#fun-collect
[p-type]:     the-prelude.md#fun-type
[p-expect]:   the-prelude.md#fun-expect
[p-assert]:   the-prelude.md#fun-assert
[p-clock]:    the-prelude.md#fun-clock
[p-rand]:     the-prelude.md#fun-rand
[p-log]:      the-prelude.md#fun-log
[p-int]:      the-prelude.md#fun-int
[p-dec]:      the-prelude.md#fun-dec
[p-sym]:      the-prelude.md#fun-sym
[p-str]:      the-prelude.md#fun-str
[p-hex]:      the-prelude.md#fun-hex
[p-oct]:      the-prelude.md#fun-oct
[p-bin]:      the-prelude.md#fun-bin
[p-keys]:     the-prelude.md#fun-keys
[p-vals]:     the-prelude.md#fun-vals
[p-pairs]:    the-prelude.md#fun-pairs
[p-seq]:      the-prelude.md#fun-seq
[p-rseq]:     the-prelude.md#fun-seq
[p-bytes]:    the-prelude.md#fun-bytes
[p-chars]:    the-prelude.md#fun-chars
[p-split]:    the-prelude.md#fun-split
[p-items]:    the-prelude.md#fun-items
[p-drange]:   the-prelude.md#fun-drange
[p-irange]:   the-prelude.md#fun-irange
[p-show]:     the-prelude.md#fun-show
[p-warn]:     the-prelude.md#fun-warn
[p-input]:    the-prelude.md#fun-input
[p-ucode]:    the-prelude.md#fun-ucode
[p-uchar]:    the-prelude.md#fun-uchar
[p-cat]:      the-prelude.md#fun-cat
[p-join]:     the-prelude.md#fun-join
[p-bcmp]:     the-prelude.md#fun-bcmp
[p-ccmp]:     the-prelude.md#fun-ccmp
[p-bsub]:     the-prelude.md#fun-bsub
[p-csub]:     the-prelude.md#fun-csub
[p-blen]:     the-prelude.md#fun-blen
[p-clen]:     the-prelude.md#fun-clen
[p-each]:     the-prelude.md#fun-each
[p-fold]:     the-prelude.md#fun-fold
[p-sep]:      the-prelude.md#fun-sep
[p-cons]:     the-prelude.md#fun-cons
[p-list]:     the-prelude.md#fun-list
[p-explode]:  the-prelude.md#fun-explode
[p-fiber]:    the-prelude.md#fun-fiber
[p-cont]:     the-prelude.md#fun-cont
[p-yield]:    the-prelude.md#fun-yield
[p-panic]:    the-prelude.md#fun-panic
[p-state]:    the-prelude.md#fun-state
[p-errval]:   the-prelude.md#fun-errval
[p-trace]:    the-prelude.md#fun-trace
[p-script]:   the-prelude.md#fun-script
[p-expr]:     the-prelude.md#fun-expr
[p-pipe]:     the-prelude.md#fun-pipe
[p-rpipe]:    the-prelude.md#fun-rpipe
[p-pump]:     the-prelude.md#fun-pump
[p-rpump]:    the-prelude.md#fun-rpump
[p-skip]:     the-prelude.md#fun-skip
[p-limit]:    the-prelude.md#fun-limit
[p-N]:        the-prelude.md#var-N  
[p-R]:        the-prelude.md#var-R
[p-L]:        the-prelude.md#var-L
[p-T]:        the-prelude.md#var-T

## API Index

- [`ten_State`][a-ten_State]
- [`ten_DatInfo`][a-ten_DatInfo]
- [`ten_DatConfig`][a-ten_DatConfig]
- [`ten_PtrInfo`][a-ten_PtrInfo]
- [`ten_PtrConfig`][a-ten_PtrConfig]
- [`ten_Tup`][a-ten_Tup]
- [`ten_Var`][a-ten_Var]
- [`ten_FunCb`][a-ten_FunCb]
- [`ten_Call`][a-ten_Call]
- [`ten_FunParams`][a-ten_FunParams]
- [`ten_MemCb`][a-ten_MemCb]
- [`ten_ErrNum`][a-ten_ErrNum]
- [`ten_ComType`][a-ten_ComType]
- [`ten_ComScope`][a-ten_ComScope]
- [`ten_FibState`][a-ten_FibState]
- [`ten_Trace`][a-ten_Trace]
- [`ten_Source`][a-ten_Source]
- [`ten_Config`][a-ten_Config]
- [`ten_Version`][a-ten_Version-0]
- [`ten_VERSION`][a-ten_VERSION-1]
- [`ten_Case`][a-ten_Case]
- [`ten_make( config, errJmp )`][a-ten_make]
- [`ten_free( ten )`][a-ten_free]
- [`ten_pushA( ten, pat, ap... )`][a-ten_pushA]
- [`ten_pushV( ten, pat, ap )`][a-ten_pushV]
- [`ten_top( ten )`][a-ten_top]
- [`ten_pop( ten )`][a-ten_pop]
- [`ten_dup( ten, tup )`][a-ten_dup]
- [`ten_size( ten, tup )`][a-ten_size]
- [`ten_def( ten, name, src )`][a-ten_def]
- [`ten_set( ten, name, src )`][a-ten_set]
- [`ten_get( ten, name, dst )`][a-ten_get]
- [`ten_type( ten, var, dst)`][a-ten_type]
- [`ten_expect( ten, what, type, var )`][a-ten_expect]
- [`ten_equal( ten, var1, var2 )`][a-ten_equal]
- [`ten_match( ten, var, tup, ... )`][a-ten_match]
- [`ten_case( WHICH, THING )`][a-ten_case]
- [`ten_END`][a-ten_END]
- [`ten_copy( ten, src, dst )`][a-ten_copy]
- [`ten_string( ten, tup )`][a-ten_string]
- [`ten_loader( ten, type, loadr, trans )`][a-ten_loader]
- [`ten_panic( ten, var )`][a-ten_panic]
- [`ten_call( ten, cls, args )`][a-ten_call]
- [`ten_call_( ten, cls, args, file, line )`][a-ten_call_]
- [`ten_yield( ten, vals )`][a-ten_yield]
- [`ten_seek( ten, ctx, size )`][a-ten_seek]
- [`ten_checkpoint( ten, cp, dst )`][a-ten_checkpoint]
- [`ten_udf( ten )`][a-ten_udf]
- [`ten_nil( ten )`][a-ten_nil]
- [`ten_log( ten, log )`][a-ten_log]
- [`ten_int( ten, int )`][a-ten_int]
- [`ten_dec( ten, dec )`][a-ten_dec]
- [`ten_sym( ten, sym )`][a-ten_sym]
- [`ten_ptr( ten, ptr )`][a-ten_ptr]
- [`ten_str( ten, str )`][a-ten_str]
- [`ten_fmtA( ten, fmt, ... )`][a-ten_fmtA]
- [`ten_fmtV( ten, fmt, ap )`][a-ten_fmtV]
- [`ten_fileSource( ten, file, name )`][a-ten_fileSource]
- [`ten_pathSource( ten, path )`][a-ten_pathSource]
- [`ten_stringSource( ten, string, name )`][a-ten_stringSource]
- [`ten_freeSource( ten, src )`][a-ten_freeSource]
- [`ten_compileScript( ten, upvals, src, scope, out, dst )`][a-ten_compileScript]
- [`ten_compileExpr( ten, upvals, src, scopr, out, dst )`][a-ten_compileExpr]
- [`ten_executeScript( ten, src, scope )`][a-ten_executeScript]
- [`ten_executeExpr( ten, src, scope )`][a-ten_executeExpr]
- [`ten_isUdf( ten, var )`][a-ten_isUdf]
- [`ten_areUdf( ten, tup )`][a-ten_areUdf]
- [`ten_setUdf( ten, var )`][a-ten_setUdf]
- [`ten_udfType( ten )`][a-ten_udfType]
- [`ten_isNil( ten, var )`][a-ten_isNil]
- [`ten_areNil( ten, tup )`][a-ten_areNil]
- [`ten_setNil( ten, var )`][a-ten_setNil]
- [`ten_nilType( ten )`][a-ten_nilType]
- [`ten_isLog( ten, var )`][a-ten_isLog]
- [`ten_setLog( ten, log, var )`][a-ten_setLog]
- [`ten_getLog( ten, var )`][a-ten_getLog]
- [`ten_logType( ten )`][a-ten_logType]
- [`ten_isInt( ten, var )`][a-ten_isInt]
- [`ten_setInt( ten, in, var )`][a-ten_setInt]
- [`ten_getInt( ten, var )`][a-ten_getInt]
- [`ten_intType( ten )`][a-ten_intType]
- [`ten_isDec( ten, var )`][a-ten_isDec]
- [`ten_setDec( ten, dec, var )`][a-ten_setDec]
- [`ten_getDec( ten, var )`][a-ten_getDec]
- [`ten_decType( ten )`][a-ten_decType]
- [`ten_isSym( ten, var )`][a-ten_isSym]
- [`ten_setSym( ten, sym, len, var )`][a-ten_setSym]
- [`ten_getSymBuf( ten, var )`][a-ten_getSymBuf]
- [`ten_getSymLen( ten, var )`][a-ten_getSymLen]
- [`ten_symType( ten )`][a-ten_symType]
- [`ten_isPtr( ten, var, info )`][a-ten_isPtr]
- [`ten_setPtr( ten, addr, info, var )`][a-ten_setPtr]
- [`ten_getPtrAddr( ten, var )`][a-ten_getPtrAddr]
- [`ten_getPtrInfo( ten, var )`][a-ten_getPtrInfo]
- [`ten_addPtrInfo( ten, config )`][a-ten_addPtrInfo]
- [`ten_ptrType( ten, info )`][a-ten_ptrType]
- [`ten_isStr( ten, var )`][a-ten_isStr]
- [`ten_newStr( ten, str, len, var )`][a-ten_newStr]
- [`ten_getStrBuf( ten, var )`][a-ten_getStrBuf]
- [`ten_getStrLen( ten, var )`][a-ten_getStrLen]
- [`ten_strType( ten )`][a-ten_strType]
- [`ten_isIdx( ten, var )`][a-ten_isIdx]
- [`ten_newIdx( ten, dst )`][a-ten_newIdx]
- [`ten_idxType( ten )`][a-ten_idxType]
- [`ten_isRec( ten, var)`][a-ten_isRec]
- [`ten_newRec( ten, idx, dst )`][a-ten_newRec]
- [`ten_recSep( ten, rec )`][a-ten_recSep]
- [`ten_recDef( ten, rec, key, src )`][a-ten_recDef]
- [`ten_recSet( ten, rec, key, src )`][a-ten_recSet]
- [`ten_recGet( ten, rec, key, dst )`][a-ten_recGet]
- [`ten_recType( ten )`][a-ten_recType]
- [`ten_isFun( ten, var )`][a-ten_isFun]
- [`ten_newFun( ten, p, dst )`][a-ten_newFun]
- [`ten_funType( ten )`][a-ten_funType]
- [`ten_isCls( ten, var )`][a-ten_isCls]
- [`ten_newCls( ten, fun, dat, dst )`][a-ten_newCls]
- [`ten_getUpvalue( ten, cls, upv, dst )`][a-ten_getUpvalue]
- [`ten_setUpvalue( ten, cls, upv, src )`][a-ten_setUpvalue]
- [`ten_clsType( ten )`][a-ten_clsType]
- [`ten_isFib( ten, var )`][a-ten_isFib]
- [`ten_newFib( ten, var )`][a-ten_newFib]
- [`ten_state( ten, fib )`][a-ten_state]
- [`ten_cont( ten, fib, args )`][a-ten_cont]
- [`ten_fibType( ten )`][a-ten_fibType]
- [`ten_isDat( ten, var, info )`][a-ten_isDat]
- [`ten_newDat( ten, info, dst )`][a-ten_newDat]
- [`ten_getDat( ten, buf, dst )`](a-ten_getDat)
- [`ten_getMember( ten, dat, mem, dst )`][a-ten_getMember]
- [`ten_setMember( ten, dat, mem, src )`][a-ten_setMember]
- [`ten_getMembers( ten, dat )`][a-ten_getMembers]
- [`ten_getDatInfo( ten, dat )`][a-ten_getDatInfo]
- [`ten_getDatBuf( ten, dat )`][a-ten_getDatBuf]
- [`ten_addDatInfo( ten, config )`][a-ten_addDatInfo]
- [`ten_datType( ten, info )`][a-ten_datType]
- [`ten_members( ten, dat )`][a-ten_members]
- [`ten_getErrNum( ten, fib )`][a-ten_getErrNum]
- [`ten_getErrVal( ten, fib )`][a-ten_getErrVal]
- [`ten_getTrace( ten, fib )`][a-ten_getTrace]
- [`ten_clearError( ten, fib )`][a-ten_clearError]
- [`ten_propError( ten, fib )`][a-ten_propError]
- [`ten_swapErrJmp( ten, errJmp )`][a-ten_swapErrJmp]
- [`ten_define`][a-ten_define]
- [`ten_fun`][a-ten_fun]
- [`ten_arg`][a-ten_arg]
- [`ten_mem`][a-ten_mem]
- [`ten_var`][a-ten_var]


[a-ten_State]:          the-api.md#type-ten_State
[a-ten_DatInfo]:        the-api.md#type-ten_DatInfo
[a-ten_DatConfig]:      the-api.md#type-ten_DatConfig
[a-ten_PtrInfo]:        the-api.md#type-ten_PtrInfo
[a-ten_PtrConfig]:      the-api.md#type-ten_PtrConfig
[a-ten_Tup]:            the-api.md#type-ten_Tup
[a-ten_Var]:            the-api.md#type-ten_Var
[a-ten_FunCb]:          the-api.md#type-ten_FunCb
[a-ten_Call]:           the-api.md#type-ten_Call
[a-ten_MemCb]:          the-api.md#type-ten_MemCb
[a-ten_FunParams]:      the-api.md#type-ten_FunParams
[a-ten_ErrNum]:         the-api.md#type-ten_ErrNum
[a-ten_ComType]:        the-api.md#type-ten_ComType
[a-ten_ComScope]:       the-api.md#type-ten_ComScope
[a-ten_FibState]:       the-api.md#type-ten_FibState
[a-ten_Trace]:          the-api.md#type-ten_Trace
[a-ten_Source]:         the-api.md#type-ten_Source
[a-ten_Config]:         the-api.md#type-ten_Config
[a-ten_Version-0]:      the-api.md#type-ten_Version
[a-ten_VERSION-1]:      the-api.md#const-ten_Version
[a-ten_Case]:           the-api.md#type-ten_Case
[a-ten_make]:           the-api.md#fun-ten_make
[a-ten_free]:           the-api.md#fun-ten_free
[a-ten_pushA]:          the-api.md#fun-ten_pushA
[a-ten_pushV]:          the-api.md#fun-ten_pushV
[a-ten_top]:            the-api.md#fun-ten_top
[a-ten_pop]:            the-api.md#fun-ten_pop
[a-ten_dup]:            the-api.md#fun-ten_dup
[a-ten_size]:           the-api.md#fun-ten_size
[a-ten_def]:            the-api.md#fun-ten_def
[a-ten_set]:            the-api.md#fun-ten_set
[a-ten_get]:            the-api.md#fun-ten_get
[a-ten_type]:           the-api.md#fun-ten_type
[a-ten_expect]:         the-api.md#fun-ten_expect
[a-ten_equal]:          the-api.md#fun-ten_equal
[a-ten_match]:          the-api.md#fun-ten_match
[a-ten_case]:           the-api.md#mac-ten_case
[a-ten_END]:            the-api.md#mac-ten_END
[a-ten_copy]:           the-api.md#fun-ten_copy
[a-ten_string]:         the-api.md#fun-ten_string
[a-ten_loader]:         the-api.md#fun-ten_loader
[a-ten_panic]:          the-api.md#fun-ten_panic
[a-ten_call]:           the-api.md#fun-ten_call
[a-ten_call_]:          the-api.md#fun-ten_call_
[a-ten_yield]:          the-api.md#fun-ten_yield
[a-ten_seek]:           the-api.md#fun-ten_seek
[a-ten_checkpoint]:     the-api.md#fun-ten_checkpoint
[a-ten_udf]:            the-api.md#fun-ten_udf
[a-ten_nil]:            the-api.md#fun-ten_nil
[a-ten_log]:            the-api.md#fun-ten_log
[a-ten_int]:            the-api.md#fun-ten_int
[a-ten_dec]:            the-api.md#fun-ten_dec
[a-ten_sym]:            the-api.md#fun-ten_sym
[a-ten_ptr]:            the-api.md#fun-ten_ptr
[a-ten_str]:            the-api.md#fun-ten_str
[a-ten_fmtA]:           the-api.md#fun-ten_fmtA
[a-ten_fmtV]:           the-api.md#fun-ten_fmtV
[a-ten_fileSource]:     the-api.md#fun-ten_fileSource
[a-ten_pathSource]:     the-api.md#fun-ten_pathSource
[a-ten_stringSource]:   the-api.md#fun-ten_stringSource
[a-ten_freeSource]:     the-api.md#fun-ten_freeSource
[a-ten_compileScript]:  the-api.md#fun-ten_compileScript
[a-ten_compileExpr]:    the-api.md#fun-ten_compileExpr
[a-ten_executeScript]:  the-api.md#fun-ten_executeScript
[a-ten_executeExpr]:    the-api.md#fun-ten_executeExpr
[a-ten_isUdf]:          the-api.md#fun-ten_isUdf
[a-ten_areUdf]:         the-api.md#fun-ten_areUdf
[a-ten_setUdf]:         the-api.md#fun-ten_setUdf
[a-ten_udfType]:        the-api.md#fun-ten_udfType
[a-ten_isNil]:          the-api.md#fun-ten_isNil
[a-ten_areNil]:         the-api.md#fun-ten_areNil
[a-ten_setNil]:         the-api.md#fun-ten_setNil
[a-ten_nilType]:        the-api.md#fun-ten_nilType
[a-ten_isLog]:          the-api.md#fun-ten_isLog
[a-ten_setLog]:         the-api.md#fun-ten_setLog
[a-ten_getLog]:         the-api.md#fun-ten_getLog
[a-ten_logType]:        the-api.md#fun-ten_logType
[a-ten_isInt]:          the-api.md#fun-ten_isInt
[a-ten_setInt]:         the-api.md#fun-ten_setInt
[a-ten_getInt]:         the-api.md#fun-ten_getInt
[a-ten_intType]:        the-api.md#fun-ten_intType
[a-ten_isDec]:          the-api.md#fun-ten_isDec
[a-ten_setDec]:         the-api.md#fun-ten_setDec
[a-ten_getDec]:         the-api.md#fun-ten_getDec
[a-ten_decType]:        the-api.md#fun-ten_decType
[a-ten_isSym]:          the-api.md#fun-ten_isSym
[a-ten_setSym]:         the-api.md#fun-ten_setSym
[a-ten_getSymBuf]:      the-api.md#fun-ten_getSymBuf
[a-ten_getSymLen]:      the-api.md#fun-ten_getSymLen
[a-ten_symType]:        the-api.md#fun-ten_symType
[a-ten_isPtr]:          the-api.md#fun-ten_isPtr
[a-ten_setPtr]:         the-api.md#fun-ten_setPtr
[a-ten_getPtrAddr]:     the-api.md#fun-ten_getPtrAddr
[a-ten_getPtrInfo]:     the-api.md#fun-ten_getPtrInfo
[a-ten_addPtrInfo]:     the-api.md#fun-ten_addPtrInfo
[a-ten_ptrType]:        the-api.md#fun-ten_ptrType
[a-ten_isStr]:          the-api.md#fun-ten_isStr
[a-ten_newStr]:         the-api.md#fun-ten_newStr
[a-ten_getStrBuf]:      the-api.md#fun-ten_getStrBuf
[a-ten_getStrLen]:      the-api.md#fun-ten_getStrLen
[a-ten_strType]:        the-api.md#fun-ten_strType
[a-ten_isIdx]:          the-api.md#fun-ten_isIdx
[a-ten_newIdx]:         the-api.md#fun-ten_newIdx
[a-ten_idxType]:        the-api.md#fun-ten_idxType
[a-ten_isRec]:          the-api.md#fun-ten_isRec
[a-ten_newRec]:         the-api.md#fun-ten_newRec
[a-ten_recSep]:         the-api.md#fun-ten_recSep
[a-ten_recDef]:         the-api.md#fun-ten_recDef
[a-ten_recSet]:         the-api.md#fun-ten_recSet
[a-ten_recGet]:         the-api.md#fun-ten_recGet
[a-ten_recType]:        the-api.md#fun-ten_recType
[a-ten_isFun]:          the-api.md#fun-ten_isFun
[a-ten_newFun]:         the-api.md#fun-ten_newFun
[a-ten_funType]:        the-api.md#fun-ten_funType
[a-ten_isCls]:          the-api.md#fun-ten_isCls
[a-ten_newCls]:         the-api.md#fun-ten_newCls
[a-ten_getUpvalue]:     the-api.md#fun-ten_getUpvalue
[a-ten_setUpvalue]:     the-api.md#fun-ten_setUpvalue
[a-ten_clsType]:        the-api.md#fun-ten_clsType
[a-ten_isFib]:          the-api.md#fun-ten_isFib
[a-ten_newFib]:         the-api.md#fun-ten_newFib
[a-ten_state]:          the-api.md#fun-ten_state
[a-ten_cont]:           the-api.md#fun-ten_cont
[a-ten_fibType]:        the-api.md#fun-ten_fibType
[a-ten_isDat]:          the-api.md#fun-ten_isDat
[a-ten_newDat]:         the-api.md#fun-ten_newDat
[a-ten_getDat]:         the-api.md#fun-ten_getDat
[a-ten_getMember]:      the-api.md#fun-ten_getMember
[a-ten_setMember]:      the-api.md#fun-ten_setMember
[a-ten_getMembers]:     the-api.md#fun-ten_getMem
[a-ten_getDatInfo]:     the-api.md#fun-ten_getDatInfo
[a-ten_getDatBuf]:      the-api.md#fun-ten_getDatBuf
[a-ten_addDatInfo]:     the-api.md#fun-ten_addDatInfo
[a-ten_datType]:        the-api.md#fun-ten_datType
[a-ten_members]:        the-api.md#fun-ten_members
[a-ten_getErrNum]:      the-api.md#fun-ten_getErrNum
[a-ten_getErrVal]:      the-api.md#fun-ten_getErrVal
[a-ten_getTrace]:       the-api.md#fun-ten_getTrace
[a-ten_clearError]:     the-api.md#fun-ten_clearError
[a-ten_propError]:      the-api.md#fun-ten_propError
[a-ten_swapErrJmp]:     the-api.md#fun-ten_swapErrJmp
[a-ten_define]:         the-api.md#mac-ten_define
[a-ten_fun]:            the-api.md#mac-ten_fun
[a-ten_arg]:            the-api.md#mac-ten_arg
[a-ten_mem]:            the-api.md#mac-ten_mem
[a-ten_var]:            the-api.md#mac-ten_var
