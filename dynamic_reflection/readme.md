# JSON Document Field Reference

This document describes **all** the major fields you may encounter in the generated JSON for both **declarations** (functions, classes, enums, fields, etc.) and **types** (built-in, pointer, function, record, etc.). Below is a comprehensive list of the typical fields, their data types, and their meaning.

---

## 1. Top-Level JSON Fields

1. **`declarations`** *(array)*  
   An array of declaration objects. Each object describes some C++ entity such as a function, a class, an enum, a variable, etc.

2. **`types`** *(array)*  
   An array of type objects. Each describes a specific kind of type (built-in, pointer, record, function, enum, etc.).

3. **`marked_declarations`** *(array of numbers)*  
   A list of integer indices referencing special declarations in the `declarations` array. Example: `[3, 5]` means you should look at `declarations[3]` and `declarations[5]` for marked declarations.

4. **`marked_types`** *(array of numbers)*  
   Similar to `marked_declarations`, but references entries in the `types` array.

5. **`type_alias_map`** *(object)*  
   A dictionary for user-friendly aliases to type IDs. Each **key** is an alias (e.g., `"std::string"`), and each **value** is the real type’s `id` (e.g., `"std::basic_string<char>"`). This is used to store or restore custom naming conventions.

6. **`name`** *(string)*  
   A user-defined or tool-defined name for this entire JSON metadata unit.

7. **`version`** *(string)*  
   A version string indicating the format or tool version.

---

## 2. Declarations Array

Each item in the `declarations` array is an **object** with various fields. Some fields are common to all declarations; others appear only with certain kinds of declarations (e.g., `EnumDecl`, `CXXRecordDecl`, etc.).

### 2.1 Common Declaration Fields

- **`__kind`** *(string)*  
  Indicates the declaration’s kind. Possible values include (but are not limited to):
  - `"ENUM_DECL"`
  - `"RECORD_DECL"`
  - `"CXX_RECORD_DECL"`
  - `"FIELD_DECL"`
  - `"FUNCTION_DECL"`
  - `"CXX_METHOD_DECL"`
  - `"CXX_CONSTRUCTOR_DECL"`
  - `"CXX_CONVERSION_DECL"`
  - `"CXX_DESTRUCTOR_DECL"`
  - `"PARAM_VAR_DECL"`
  - `"VAR_DECL"`
  - ...  
  It’s essentially the enumerator name of the internal declaration kind.

- **`id`** *(string)*  
  A globally or uniquely identifiable string for this declaration (e.g., a USR from Clang, a tool-generated ID, etc.).

- **`index`** *(number)*  
  The array index of this object in `declarations`. Used to cross-reference with `marked_declarations` or other references.

- **`fq_name`** *(string)*  
  A fully qualified name, possibly including namespaces, class scopes, or function parameter signatures.

- **`name`** *(string)*  
  The simple identifier without namespaces or parameter lists (e.g., just `"TestFunction"` or `"TestClass"`).

- **`spelling`** *(string)*  
  The raw spelling or textual representation in the source code. It can be identical to `name` or might include more details (especially for templated items or operators).

- **`is_anonymous`** *(boolean)*  
  Indicates if the declaration is anonymous (e.g., an unnamed enum or struct).

- **`type_id`** *(string)*  
  References a type from the `types` array by that type’s `id`. For example, a field declaration might have `"type_id": "int"`, which corresponds to a built-in type object in the `types` array.

- **`attributes`** *(array of strings)*  
  Extra metadata or user-defined markers on this declaration (e.g., `[ "marked" ]`). This can be empty or omitted if none exist.

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
  A list of IDs referencing `FIELD_DECL` objects.
- **`is_abstract`** *(boolean)*  
  True if this record has at least one pure virtual function.

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
  True if it’s declared as a const member function (C++).
- **`is_volatile`** *(boolean)*  
  True if it’s declared volatile.

#### 2.2.5 `ParmVarDecl`

- **`arg_index`** *(number)*  
  The index of the parameter in the function’s parameter list (0-based).

#### 2.2.6 `VarDecl`

- Usually contains only the shared fields (`id`, `type_id`, `attributes`, etc.) for a global or local variable. Does not have the same function-related fields as `ParmVarDecl`.

---

## 3. Types Array

Each entry in the `types` array is an **object** describing a particular type. Shared fields:

- **`__kind`** *(string)*  
  The type category, for instance:
  - `"BuiltinType"`
  - `"PointerType"`
  - `"LvalueReferenceType"`
  - `"RvalueReferenceType"`
  - `"RecordType"`
  - `"EnumType"`
  - `"FunctionType"`
  - `"UnsupportedType"`
  - etc.

- **`id`** *(string)*  
  A unique type identifier.

- **`index`** *(number)*  
  Its position in the `types` array.

- **`name`** *(string)*  
  A descriptive or user-friendly name for the type. It can differ from `id` if there’s an alias or a custom label.

- **`type_kind`** *(number)*  
  An integer enumerating the internal kind. For example:
  - `2` => Builtin
  - `24` => LvalueReference
  - `25` => RvalueReference
  - `29` => PointerToData (or MemberDataPointer)
  - `30` => PointerToMemberFunction
  - `32` => Function
  - `34` => UnscopedEnum
  - `35` => ScopedEnum
  - `36` => Record
  - etc.  
  (Exact values can vary depending on the internal tool.)

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

## 4. Marked Declarations & Types

- **`marked_declarations`** *(array of numbers)*  
  These are integers that match the `index` of some declaration in the `declarations` array. They denote special or user-marked declarations.

- **`marked_types`** *(array of numbers)*  
  Similarly, these integers match the `index` of some type in the `types` array.

Example usage:
```json
"marked_declarations": [6, 12],
"marked_types": [12, 17, 30]
