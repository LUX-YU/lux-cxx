# JSON Schema Reference

This document describes **all** the major fields you may encounter in the generated JSON for both **declarations** (functions, classes, enums, fields, etc.) and **types** (built-in, pointer, function, record, etc.). Below is a comprehensive list of the typical fields, their data types, and their meaning.

---

## 1. Top-Level JSON Fields

1. **`declarations`** *(array)*  
   An array of declaration objects. Each object describes some C++ entity such as a function, a class, an enum, a variable, etc.

2. **`types`** *(array)*  
   An array of type objects. Each describes a specific kind of type (built-in, pointer, record, function, enum, etc.).

3. **`marked_record_decls`** *(array of numbers)*  
   Integer indices into `declarations` for every `LUX_META(<marker>)`-tagged class/struct/union.

4. **`marked_function_decls`** *(array of numbers)*  
   Integer indices into `declarations` for every marked free function (or function template).

5. **`marked_enum_decls`** *(array of numbers)*  
   Integer indices into `declarations` for every marked enum.

   > Older revisions documented `marked_declarations` / `marked_types`; those fields **do not exist** in the current schema. Marked entries are split into the three lists above and there is no `marked_types`.

6. **`type_alias_map`** *(object)*  
   A dictionary for user-friendly aliases to type IDs. Each **key** is an alias (e.g., `"std::string"`), and each **value** is the real type's `id` (e.g., `"std::basic_string<char>"`).

7. **`name`** *(string)*  
   A user-defined or tool-defined name for this entire JSON metadata unit.

8. **`version`** *(string)*  
   A version string indicating the format or tool version.

---

## 2. Declarations Array

Each item in the `declarations` array is an **object** with various fields. Some fields are common to all declarations; others appear only with certain kinds of declarations (e.g., `EnumDecl`, `CXXRecordDecl`, etc.).

### 2.1 Common Declaration Fields

- **`__kind`** *(string)*  
  Indicates the declaration's kind. Produced by `declKindToString`. Possible values:
  - `"EnumDecl"`
  - `"RecordDecl"`
  - `"CXXRecordDecl"`
  - `"FieldDecl"`
  - `"FunctionDecl"`
  - `"CXXMethodDecl"`
  - `"CXXConstructorDecl"`
  - `"CXXConversionDecl"`
  - `"CXXDestructorDecl"`
  - `"ParmVarDecl"`
  - `"VarDecl"`
  - `"UnknownDecl"` (fallback)

- **`id`** *(string)*  
  A globally unique string for this declaration (typically a Clang USR).

- **`index`** *(number)*  
  The array index of this object in `declarations`. `marked_*_decls` arrays reference declarations by this index. Equal to `static_cast<size_t>(-1)` for default-constructed declarations.

- **`hash`** *(string)*  
  64-bit FNV-1a hash of `id`, written as a decimal string (avoids JS number-precision loss when round-tripping through other tools).

- **`tag_kind`** *(string, only on `EnumDecl` / `RecordDecl` / `CXXRecordDecl`)*  
  `"Struct"`, `"Class"`, `"Union"`, or `"Enum"`. Distinguishes `struct Foo` from `class Foo` after deserialization.

- **`fq_name`** *(string)*  
  Fully qualified name, including namespaces / class scopes / function parameter signatures.

- **`name`** *(string)*  
  Clang's `displayName` for the cursor (often includes parameter list for functions).

- **`spelling`** *(string)*  
  Raw spelling — typically the bare identifier without parameter lists.

- **`is_anonymous`** *(boolean)*  
  Indicates if the declaration is anonymous (e.g., an unnamed enum or struct).

- **`type_id`** *(string)*  
  References a type from the `types` array by that type's `id`. For example, a field declaration might have `"type_id": "int"`.

- **`attributes`** *(array of strings)*  
  Parsed annotation payload from `LUX_META(...)`. Semicolons inside the macro split into separate entries (e.g. `LUX_META(a;b)` → `["a", "b"]`).

#### 2.1.1 Visibility

In many declarations (especially in C++ classes), you may see a **`visibility`** field:
- **`visibility`** *(number)*  
  - Typically `1` = public, `2` = protected, `3` = private, `0` = invalid/unspecified.  
  - Applies to fields, methods, constructors, etc.

---

### 2.2 Specific Declaration Kinds

Below are the additional or unique fields you may see for particular declaration kinds.

#### 2.2.1 `EnumDecl`

- **`is_scoped`** *(boolean)*  
  Whether the enum is a scoped enum (`enum class`) or not.
- **`underlying_type_id`** *(string)*  
  The type ID of its underlying type. Usually references a built-in type like `"int"` in the `types` array.
- **`enumerators`** *(array)*  
  Each enumerator is an object with:
  - `name` (e.g., `"VALUE1"`)
  - `signed_value` (e.g., `0`)
  - `unsigned_value` (e.g., `0`)

#### 2.2.2 `CXXRecordDecl` (or `RECORD_DECL`)

- **`bases`** *(array of strings)*  
  Class IDs (via `id`) of direct base classes (applies to C++).
- **`constructor_decls`** *(array of strings)*  
  A list of IDs referencing `CXX_CONSTRUCTOR_DECL` objects.
- **`destructor_decl`** *(string)*  
  A single reference ID to the `CXX_DESTRUCTOR_DECL`, if present.
- **`method_decls`** *(array of strings)*  
  A list of IDs referencing non-static member functions (`CXX_METHOD_DECL`, `CXX_CONVERSION_DECL`, etc.) that are not constructors or destructors.
- **`static_method_decls`** *(array of strings)*  
  A list of IDs referencing static member functions.
- **`field_decls`** *(array of strings)*  
  A list of IDs referencing `FieldDecl` objects.
- **`is_abstract`** *(boolean)*  
  True if this record has at least one pure virtual function.
- **`is_template`** *(boolean)*  
  True when this was parsed from a class/struct template declaration (i.e. cursor kind `CXCursor_ClassTemplate`).
- **`template_params`** *(array, present only if `is_template` is true)*  
  Each element describes one template parameter:
  - `kind` *(string)* — `"type"`, `"non_type"`, or `"template_tmpl"`
  - `name` *(string)* — e.g. `"T"`, `"N"`, `"Alloc"`
  - `spelling` *(string)* — e.g. `"typename T"`, `"int N"`

#### 2.2.3 `FieldDecl`

- **`parent_class_id`** *(string)*  
  ID of the containing class or struct.
- **`offset`** *(number)*  
  The bit offset within the class or struct layout (if known).
- **`visibility`** *(number)*  
  The access level (e.g., 1 = public, etc.).

#### 2.2.4 `FunctionDecl` / `CXXMethodDecl` / `CXXConstructorDecl` / `CXXDestructorDecl`

Many fields overlap among these function-like declarations:

- **`mangling`** *(string)*  
  The mangled name generated by the compiler (if available).
- **`is_variadic`** *(boolean)*  
  True if the function has a `...` parameter.
- **`result_type_id`** *(string)*  
  The function’s return type ID in `types`.
- **`params`** *(array of strings)*  
  A list of parameter declaration IDs (these IDs should point to `ParmVarDecl` entries).
- **`parent_class_id`** *(string)*  
  For methods, the ID of the owning class (if it is a member function).
- **`is_static`** *(boolean)*  
  True for static member functions.
- **`is_virtual`** *(boolean)*  
  True for virtual member functions.
- **`is_const`** *(boolean)*  
  True if it's declared as a const member function (C++).
- **`is_volatile`** *(boolean)*  
  True if it's declared volatile.
- **`invoke_name`** *(string)*  
  A callable name for the function. For free functions, this is typically `namespace::spelling` (the `fq_name` minus the parameter list). For member functions it's `spelling`. Generators commonly use it to synthesize `&Class::method` expressions.
- **`is_template`** *(boolean, on `FunctionDecl` only)*  
  True when parsed from `CXCursor_FunctionTemplate`. Follows the same shape as on `CXXRecordDecl`.
- **`template_params`** *(array, present only when `is_template` is true)*  
  Same schema as on `CXXRecordDecl`.
#### 2.2.5 `ParmVarDecl`

- **`arg_index`** *(number)*  
  The index of the parameter in the function’s parameter list (0-based).

#### 2.2.6 `VarDecl`

- Usually contains only the shared fields (`id`, `type_id`, `attributes`, etc.) for a global or local variable. Does not have the same function-related fields as `ParmVarDecl`.

---

## 3. Types Array

Each entry in the `types` array is an **object** describing a particular type. Shared fields:

- **`__kind`** *(string)*  
  The type category. Produced by `typeKindToString`. Possible values:
  - `"BuiltinType"`
  - `"PointerType"` — generic pointer
  - `"ObjectPointerType"` — pointer-to-object (`int*`)
  - `"FuncPointerType"` — pointer-to-function (`void(*)(int)`)
  - `"MemberDataPointerType"` — pointer-to-data-member (`int C::*`)
  - `"MemberFuncPointerType"` — pointer-to-member-function (`int (C::*)()`)
  - `"LValueReferenceType"`
  - `"RValueReferenceType"`
  - `"RecordType"`
  - `"EnumType"`, `"ScopedEnumType"`, `"UnscopedEnumType"`
  - `"FunctionType"`
  - `"UnsupportedType"` (fallback)

- **`id`** *(string)*  
  A unique type identifier.

- **`index`** *(number)*  
  Its position in the `types` array.

- **`name`** *(string)*  
  A descriptive or user-friendly name for the type. It can differ from `id` if there’s an alias or a custom label.

- **`is_const`** *(boolean)*  
  True if `const`.

- **`is_volatile`** *(boolean)*  
  True if `volatile`.

- **`size`** *(number)*  
  Size in bytes if known. May be `-2` or `0` if unknown or not applicable.

- **`align`** *(number)*  
  Alignment in bytes if known. Similarly may be `-2` or `0` if unknown.

Below are the possible subtype fields depending on `__kind`.

### 3.1 `BuiltinType`

- **`builtin_type`** *(number)*  
  An internal code for the built-in category (e.g., 17 => `int`, 22 => `double`, etc.).  
  Typically used for fundamental types like `int`, `bool`, `float`, etc.

### 3.2 `PointerType`

This covers a range of pointer categories (pointer to object, pointer to function, pointer to member, etc.). The JSON might unify them under `"PointerType"` or a similarly derived name.

- **`pointee_id`** *(string)*  
  The `id` of the type being pointed to.
- **`is_pointer_to_member`** *(boolean)*  
  True if this pointer type is specifically a pointer-to-member (e.g., `int MyClass::*`).

### 3.3 `LValueReferenceType` / `RValueReferenceType`

- **`referred_id`** *(string)*  
  The `id` of the type to which this reference refers (e.g., `int` -> `int&` or `int&&`).

### 3.4 `RecordType`

- **`decl_id`** *(string)*  
  References a `CXXRecordDecl` or `RecordDecl` in `declarations` by `id`.
- **`template_name`** *(string, optional)*  
  Present when the type is a template specialization. Contains the template name without arguments (e.g., `"std::vector"`, `"std::array"`).
- **`template_arguments`** *(array, optional)*  
  Present when the type is a template specialization. An ordered list of template argument objects, each containing:
  - **`kind`** *(string)* — `"Type"` or `"Integral"`
  - **`spelling`** *(string)* — Human-readable representation (e.g., `"int"`, `"float"`, `"3"`)
  - **`type_id`** *(string, for kind=Type)* — References a type in the `types` array
  - **`integral_value`** *(number, for kind=Integral)* — The compile-time integer value

  Example for `std::array<float, 3>`:
  ```json
  "template_name": "std::array",
  "template_arguments": [
      { "kind": "Type", "spelling": "float", "type_id": "float" },
      { "kind": "Integral", "spelling": "3", "integral_value": 3 }
  ]
  ```

### 3.5 `EnumType`

- **`decl_id`** *(string)*  
  References an `EnumDecl` in `declarations` by `id`.

### 3.6 `FunctionType`

- **`result_type_id`** *(string)*  
  The return type’s `id`.
- **`param_types`** *(array of strings)*  
  A list of type IDs for each parameter.
- **`is_variadic`** *(boolean)*  
  True if this function type is variadic (`...`).

### 3.7 `UnsupportedType`

- Used when the tool encounters a type that it cannot handle. May contain only the basic fields like `id`, `name`, `__kind`, etc.

---

## 4. Marked Declarations

The parser splits marked declarations into three index lists by kind:

- **`marked_record_decls`** *(array of numbers)* — indices into `declarations` for `CXXRecordDecl`/`RecordDecl` entries
- **`marked_function_decls`** *(array of numbers)* — indices for `FunctionDecl` entries
- **`marked_enum_decls`** *(array of numbers)* — indices for `EnumDecl` entries

Each integer is the `index` field of some declaration. There is **no** `marked_types` list — types are only marked transitively through the declarations that reference them.

Example:
```json
"marked_record_decls":   [6, 12],
"marked_function_decls": [3],
"marked_enum_decls":     [17, 30]
```
