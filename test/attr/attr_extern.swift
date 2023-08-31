// RUN: %target-typecheck-verify-swift

@_extern(wasm, module: "m1", name: "f1")
func f1(x: Int) -> Int

@_extern(wasm, module: "m2", name: ) // expected-error  {{expected string literal in '_extern' attribute}} expected-error{{expected declaration}}
func f2ErrorOnMissingNameLiteral(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}}

@_extern(wasm, module: "m3", name) // expected-error  {{expected ':' after label 'name'}} expected-error{{expected declaration}}
func f3ErrorOnMissingNameColon(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}}

@_extern(wasm, module: "m3",) // expected-error  {{expected name argument to @_extern attribute}} expected-error{{expected declaration}}
func f3ErrorOnMissingNameLabel(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}}

@_extern(wasm, module: "m4") // expected-error {{expected name argument to @_extern attribute}} expected-error{{expected declaration}}
func f3ErrorOnMissingName(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}}

@_extern(wasm, module: ) // expected-error {{expected string literal in '_extern' attribute}} expected-error{{expected declaration}}
func f4ErrorOnMissingModuleLiteral(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}}

@_extern(wasm, module) // expected-error {{expected ':' after label 'module'}} expected-error{{expected declaration}}
func f5ErrorOnMissingModuleColon(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}} 

@_extern(wasm,) // expected-error {{expected module argument to @_extern attribute}} expected-error{{expected declaration}}
func f6ErrorOnMissingModuleLabel(x: Int) -> Int // expected-error{{expected '{' in body of function declaration}} 