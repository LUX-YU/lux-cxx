# MetaUnit JSON Field Documentation (Refactored Version)

This document provides a detailed description of the fields used in the JSON files produced by the MetaUnit serialization process. It explains the JSON structure and the purpose of each field, along with examples to help clarify the format.

---

## 1. Overall JSON Structure

The MetaUnit JSON file comprises the following key parts:

- **declarations**  
  - **Type:** Array of objects  
  - **Description:** Contains all serialized declaration objects, each corresponding to a serialized instance of a `Decl` subclass.

- **types**  
  - **Type:** Array of objects  
  - **Description:** Contains all serialized type objects, each corresponding to a serialized instance of a `Type` subclass.

- **type_alias_map**  
  - **Type:** Object (key/value map)  
  - **Description:** Maps type alias strings (keys) to the corresponding type ID (values). An empty string may be used if a type is unavailable.

- **marked_declarations**  
  - **Type:** Array of integers  
  - **Description:** Contains indices pointing to the declarations in the `declarations` array that require special handling.

- **marked_types**  
  - **Type:** Array of integers  
  - **Description:** Contains indices pointing to the types in the `types` array that have been "marked."

- **version**  
  - **Type:** String  
  - **Description:** The version string of the MetaUnit.

- **name**  
  - **Type:** String  
  - **Description:** The name of the MetaUnit.

### 1.1 Additional Generator Fields
These fields are added by the generator but are not part of the standard MetaUnit JSON:
- **source_path**: The path of the source file.
- **source_parent**: The directory path of the source file.
- **parser_compile_options**: The compiler options used for parsing.

#### Example: Overall JSON Overview
```json
{
  "declarations": [ ... ],
  "types": [ ... ],
  "type_alias_map": { "MyInt": "int" },
  "marked_declarations": [0, 2, 5],
  "marked_types": [1, 3],
  "name": "example_meta_unit",
  "version": "1.0.0",
  "source_path": "src/module.cpp",
  "source_parent": "src",
  "parser_compile_options": "-std=c++17"
}
```

---

## 2. Declaration Objects

Declaration objects represent various kinds of declarations. Each declaration includes a set of common fields, as well as specialized fields based on the declaration type.

### 2.1 Common Declaration Fields

- **id**  
  - **Type:** String  
  - **Description:** A unique identifier for the declaration.

- **__kind**  
  - **Type:** String  
  - **Description:** A string indicating the type of declaration (e.g., `"ENUM_DECL"`, `"FUNCTION_DECL"`).

- **name**  
  - **Type:** String  
  - **Description:** The simple name of the declaration (applicable to instances inheriting from `NamedDecl`).

- **fq_name**  
  - **Type:** String  
  - **Description:** The fully-qualified name.

- **spelling**  
  - **Type:** String  
  - **Description:** The spelling of the declaration.

- **attributes**  
  - **Type:** Array of strings  
  - **Description:** Attributes associated with the declaration.

- **is_anonymous**  
  - **Type:** Boolean  
  - **Description:** Indicates whether the declaration is anonymous.

- **type_id**  
  - **Type:** String  
  - **Description:** The ID of the associated type (if any).

#### Example: Function Declaration
```json
{
  "__kind": "FunctionDecl",
  "id": "func_1",
  "name": "compute",
  "fq_name": "namespace::compute",
  "spelling": "compute",
  "attributes": ["LUX::META", "static_reflection"],
  "is_anonymous": false,
  "type_id": "void (int, int)",
  "is_variadic": false,
  "mangling": "mangled_name_here",
  "params": ["param_1", "param_2"],
  "result_type_id": "void"
}
```

### 2.2 Specialized Declaration Fields

#### 2.2.1 ENUM_DECL (Enumeration Declaration)

- **is_scoped**  
  - **Type:** Boolean  
  - **Description:** Indicates whether the enum is scoped.

- **underlying_type_id**  
  - **Type:** String  
  - **Description:** The type ID of the enum’s underlying type.

- **enumerators**  
  - **Type:** Array of objects  
  - **Description:** Contains each enumerator with:
    - **name**: The name of the enumerator.
    - **signed_value**: The signed integer value.
    - **unsigned_value**: The unsigned integer value.

##### Example: Enumeration Declaration
```json
{
  "__kind": "EnumDecl",
  "id": "enum_1",
  "name": "TestEnum",
  "fq_name": "TestEnum",
  "spelling": "TestEnum",
  "attributes": ["LUX::META"],
  "is_anonymous": false,
  "is_scoped": true,
  "type_id": "TestEnum",
  "underlying_type_id": "int",
  "enumerators": [
    { "name": "VALUE1", "signed_value": 0, "unsigned_value": 0 },
    { "name": "VALUE2", "signed_value": 100, "unsigned_value": 100 }
  ]
}
```

#### 2.2.2 RECORD_DECL and CXX_RECORD_DECL (Record/Class Declarations)

- **For RECORD_DECL (Record Declaration):**  
  - No additional fields are added.

- **For CXX_RECORD_DECL (C++ Record Declaration):**
  - **bases**: Array of base class IDs.
  - **constructor_decls**: Array of constructor declaration IDs.
  - **destructor_decl**: The ID of the destructor declaration.
  - **method_decls**: Array of non-static method declaration IDs.
  - **static_method_decls**: Array of static method declaration IDs.
  - **field_decls**: Array of field declaration IDs.
  - **is_abstract**: A Boolean flag indicating whether the class is abstract.

##### Example: C++ Record Declaration
```json
{
  "__kind": "CXXRecordDecl",
  "id": "class_1",
  "name": "TestStruct",
  "fq_name": "TestStruct",
  "spelling": "TestStruct",
  "attributes": ["LUX::META"],
  "is_anonymous": false,
  "is_abstract": false,
  "bases": [],
  "constructor_decls": [],
  "destructor_decl": "",
  "method_decls": [],
  "static_method_decls": [],
  "field_decls": ["field_1", "field_2"],
  "type_id": "TestStruct"
}
```

#### 2.2.3 FIELD_DECL (Field Declaration)

- **visibility**  
  - **Type:** Integer  
  - **Description:** The visibility level of the field (e.g., 1 for public).

- **offset**  
  - **Type:** Number  
  - **Description:** The offset (in bytes) of the field within its parent structure/class.

- **parent_class_id**  
  - **Type:** String  
  - **Description:** The ID of the parent class (if applicable).

##### Example: Field Declaration
```json
{
  "__kind": "FieldDecl",
  "id": "field_1",
  "name": "a1",
  "fq_name": "TestStruct::a1",
  "spelling": "a1",
  "attributes": [],
  "is_anonymous": false,
  "type_id": "int",
  "offset": 0,
  "parent_class_id": "TestStruct",
  "visibility": 1
}
```

#### 2.2.4 FUNCTION_DECL and Related Declarations (e.g., CXX_METHOD_DECL)

Function declarations include additional fields beyond the common ones:

- **result_type_id**  
  - **Type:** String  
  - **Description:** The type ID of the return value.

- **mangling**  
  - **Type:** String  
  - **Description:** The mangled name of the function.

- **is_variadic**  
  - **Type:** Boolean  
  - **Description:** Indicates whether the function accepts a variable number of arguments.

- **params**  
  - **Type:** Array of strings  
  - **Description:** An array of parameter declaration IDs.

##### Example: Function Declaration
```json
{
  "__kind": "FunctionDecl",
  "id": "func_2",
  "name": "TestFunction",
  "fq_name": "TestFunction(int)",
  "spelling": "TestFunction",
  "attributes": ["LUX::META"],
  "is_anonymous": false,
  "is_variadic": false,
  "mangling": "mangled_func_name",
  "params": ["param_1", "param_2"],
  "result_type_id": "int",
  "type_id": "int (int, double)"
}
```

#### 2.2.5 PARAM_VAR_DECL / VAR_DECL

- **PARAM_VAR_DECL:** Represents parameter variable declarations with an extra field **index** indicating the parameter's position in the list.
- **VAR_DECL:** Represents variable declarations with no additional fields.

---

## 3. Type Objects

Type objects store metadata about types and consist of common fields along with specialized fields based on the type kind.

### 3.1 Common Type Fields

- **id**  
  - **Type:** String  
  - **Description:** Unique identifier for the type.

- **__kind**  
  - **Type:** String  
  - **Description:** The type category (e.g., `"BuiltinType"`, `"Pointer"`, `"RecordType"`, etc.).

- **name**  
  - **Type:** String  
  - **Description:** The name of the type.

- **type_kind**  
  - **Type:** Integer  
  - **Description:** A numerical representation of the type category.

- **is_const**  
  - **Type:** Boolean  
  - **Description:** Indicates if the type is constant.

- **is_volatile**  
  - **Type:** Boolean  
  - **Description:** Indicates if the type is volatile.

- **size**  
  - **Type:** Integer  
  - **Description:** The size (in bytes) of the type.

- **align**  
  - **Type:** Integer  
  - **Description:** The alignment (in bytes) required for the type.

#### Example: Builtin Type
```json
{
  "__kind": "BuiltinType",
  "id": "int",
  "name": "int",
  "builtin_type": 17,
  "type_kind": 2,
  "is_const": false,
  "is_volatile": false,
  "size": 4,
  "align": 4
}
```

### 3.2 Specialized Type Fields

#### 3.2.1 Builtin Types
- **builtin_type:** An integer representing the specific builtin type (e.g., int, char).

#### 3.2.2 Pointer Types
- **pointee_id**  
  - **Type:** String  
  - **Description:** The ID of the type being pointed to.
- **is_pointer_to_member**  
  - **Type:** Boolean  
  - **Description:** Indicates if this is a pointer-to-member.

#### 3.2.3 Reference Types (LvalueReference / RvalueReference)
- **referred_id**  
  - **Type:** String  
  - **Description:** The ID of the referenced type.

#### 3.2.4 Record/Enum Types
- **decl_id**  
  - **Type:** String  
  - **Description:** The ID of the associated record or enum declaration.

#### 3.2.5 Function Types
- **result_type_id**  
  - **Type:** String  
  - **Description:** The ID of the function's return type.
- **is_variadic**  
  - **Type:** Boolean  
  - **Description:** Indicates if the function type accepts a variable number of parameters.
- **param_types**  
  - **Type:** Array of strings  
  - **Description:** A list of type IDs for each parameter.

##### Example: Function Type
```json
{
  "__kind": "FunctionType",
  "id": "void (int, double)",
  "name": "void (int, double)",
  "func_decl_id": "func_decl_3",
  "param_types": ["int", "double"],
  "result_type_id": "void",
  "is_variadic": false,
  "type_kind": 40,
  "size": 1,
  "align": 4
}
```

### 3.3 UnsupportedType

Some types that cannot be fully represented are marked as `UnsupportedType`.

##### Example: Unsupported Type
```json
{
