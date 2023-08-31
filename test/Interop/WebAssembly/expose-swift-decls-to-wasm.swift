// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %s -emit-ir -module-name Expose | %FileCheck %s

import Swift

// CHECK: define {{.*}} void @"$s6Expose8exposed1yyF"() [[EA1:#[0-9]+]]
@_expose(wasm)
public func exposed1() {
}

// CHECK: define {{.*}} void @"$s6Expose18exposed2NotExposedyyF"()
public func exposed2NotExposed() {
}

// CHECK: define {{.*}} void @"$s6Expose22exposed3WithCustomNameyyF"() [[EA3:#[0-9]+]]
@_expose(wasm, "exposed3_with_custom_name")
public func exposed3WithCustomName() {
}


// CHECK: define {{.*}} void @"$s6Expose17exposed4NonCIdentyyF"() [[EA4:#[0-9]+]]
@_expose(wasm, "exposed4-non-c-ident")
public func exposed4NonCIdent() {
}

// CHECK: define {{.*}} i32 @"$s6Expose17exposed5ReturnInts5Int32VyF"() [[EA5:#[0-9]+]]
@_expose(wasm)
public func exposed5ReturnInt() -> Int32 {
  return 0
}

// CHECK: define {{.*}} i32 @"$s6Expose17exposed6WithCDecls5Int32VyF"()
// CHECK: define {{.*}} i32 @exposed6WithCDecl() [[EA6:#[0-9]+]]
@_expose(wasm)
@_cdecl("exposed6WithCDecl")
public func exposed6WithCDecl() {
}

// CHECK: attributes [[EA1]] = {{{.*}} "wasm-export-name"="$s6Expose8exposed1yyF" {{.*}}}
// CHECK-NOT: attributes {{.*}} = {{{.*}} "wasm-export-name"="$s6Expose18exposed2NotExposedyyF" {{.*}}}
// CHECK: attributes [[EA3]] = {{{.*}} "wasm-export-name"="exposed3_with_custom_name" {{.*}}}
// CHECK: attributes [[EA4]] = {{{.*}} "wasm-export-name"="exposed4-non-c-ident" {{.*}}}
// CHECK: attributes [[EA5]] = {{{.*}} "wasm-export-name"="$s6Expose17exposed5ReturnInts5Int32VyF" {{.*}}}
// CHECK: attributes [[EA6]] = {{{.*}} "wasm-export-name"="$s6Expose17exposed6WithCDecls5Int32VyF" {{.*}}}
