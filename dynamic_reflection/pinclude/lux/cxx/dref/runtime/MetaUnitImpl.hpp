#pragma once
#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/algotithm/hash.hpp>
#include <lux/cxx/lan_model/types/arithmetic.hpp>
#include <lux/cxx/lan_model/types/array.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>
#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/types/member_pointer_type.hpp>
#include <lux/cxx/lan_model/types/pointer_type.hpp>
#include <lux/cxx/lan_model/types/reference_type.hpp>

namespace lux::cxx::dref
{
	struct MetaUnitData
	{
		std::vector<Declaration*>						_markable_decl_list;
		std::unordered_map<std::size_t, Declaration*>	_markable_decl_map;

		// for buff
		std::vector<Declaration*>						_unmarkable_decl_list;
		std::unordered_map<std::size_t, Declaration*>	_unmarkable_decl_map;

		std::vector<TypeMeta*>							_meta_type_list;
		std::unordered_map<std::size_t, TypeMeta*>		_meta_type_map;
	};

	constexpr auto& hash = lux::cxx::algorithm::hash;

	static inline void type_delete(TypeMeta* meta)
	{
		using namespace ::lux::cxx::lan_model;

        delete[] meta->name;
		
		switch (meta->type_kind)
		{
			case ETypeKind::IMCOMPLETE:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::IMCOMPLETE>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::FUNDAMENTAL:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::FUNDAMENTAL>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::VOID:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::VOID>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::NULLPTR_T:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::NULLPTR_T>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::ARITHMETIC:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::ARITHMETIC>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::INTEGRAL:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::INTEGRAL>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::BOOL:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::BOOL>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::CHARACTER:

			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::CHARACTER>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::NARROW_CHARACTER:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::NARROW_CHARACTER>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::ORDINARY_CHARACTER:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::ORDINARY_CHARACTER>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::CHAR:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::CHAR>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::SIGNED_CHAR:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::SIGNED_CHAR>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_CHAR:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_CHAR>::type*>(meta);
				delete type_meta;
				break;
			}
#if __cplusplus > 201703L // only support c++20
			case ETypeKind::CHAR8T:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::CHAR8T>::type*>(meta);
				delete type_meta;
				break;
			}
#endif
			case ETypeKind::WIDE_CHARACTER:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::WIDE_CHARACTER>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::CHAR16_T:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::CHAR16_T>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::CHAR32_T:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::CHAR32_T>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::WCHAR_T:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::WCHAR_T>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::SIGNED_INTEGRAL:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::SIGNED_INTEGRAL>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::STANDARD_SIGNED:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::STANDARD_SIGNED>::type*>(meta);
				delete type_meta;
				break;

			}
			case ETypeKind::SHORT_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::SHORT_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::LONG_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::LONG_INT>::type*>(meta);

				delete type_meta;
				break;
			}
			case ETypeKind::LONG_LONG_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::LONG_LONG_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::EXTENDED_SIGNED:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::EXTENDED_SIGNED>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_INTEGRAL:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_INTEGRAL>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::STANDARD_UNSIGNED:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::STANDARD_UNSIGNED>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_SHORT_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_SHORT_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_LONG_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_LONG_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNSIGNED_LONG_LONG_INT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNSIGNED_LONG_LONG_INT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::EXTENDED_UNSIGNED:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::EXTENDED_UNSIGNED>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::FLOAT_POINT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::FLOAT_POINT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::STANDARD_FLOAT_POINT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::STANDARD_FLOAT_POINT>::type*>(meta);

				delete type_meta;
				break;
			}
			case ETypeKind::FLOAT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::FLOAT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::DOUBLE:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::DOUBLE>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::LONG_DOUBLE:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::LONG_DOUBLE>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::EXTENDED_FLOAT_POINT:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::EXTENDED_FLOAT_POINT>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::COMPOUND:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::COMPOUND>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::REFERENCE:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::REFERENCE>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::LVALUE_REFERENCE:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::LVALUE_REFERENCE>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::LVALUE_REFERENCE_TO_OBJECT:
			{
				delete static_cast<typename type_kind_map<ETypeKind::LVALUE_REFERENCE_TO_OBJECT>::type*>(meta);
				break;
			}
			case ETypeKind::LVALUE_REFERENCE_TO_FUNCTION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::LVALUE_REFERENCE_TO_FUNCTION>::type*>(meta);
				break;
			}
			case ETypeKind::RVALUE_REFERENCE:
			{
				delete static_cast<typename type_kind_map<ETypeKind::RVALUE_REFERENCE>::type*>(meta);
				break;
			}
			case ETypeKind::RVALUE_REFERENCE_TO_OBJECT:
			{
				delete static_cast<typename type_kind_map<ETypeKind::RVALUE_REFERENCE_TO_OBJECT>::type*>(meta);
				break;
			}
			case ETypeKind::RVALUE_REFERENCE_TO_FUNCTION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::RVALUE_REFERENCE_TO_FUNCTION>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER_TO_OBJECT:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER_TO_OBJECT>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER_TO_FUNCTION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER_TO_FUNCTION>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER_TO_MEMBER:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER_TO_MEMBER>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER_TO_DATA_MEMBER:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER_TO_DATA_MEMBER>::type*>(meta);
				break;
			}
			case ETypeKind::POINTER_TO_MEMBER_FUNCTION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::POINTER_TO_MEMBER_FUNCTION>::type*>(meta);
				break;
			}
			case ETypeKind::ARRAY:
			{
				delete static_cast<typename type_kind_map<ETypeKind::ARRAY>::type*>(meta);
				break;
			}
			case ETypeKind::FUNCTION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::FUNCTION>::type*>(meta);
				break;
			}
			case ETypeKind::ENUMERATION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::ENUMERATION>::type*>(meta);
				break;
			}
			case ETypeKind::UNSCOPED_ENUMERATION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::UNSCOPED_ENUMERATION>::type*>(meta);
				break;
			}
			case ETypeKind::SCOPED_ENUMERATION:
			{
				delete static_cast<typename type_kind_map<ETypeKind::SCOPED_ENUMERATION>::type*>(meta);
				break;
			}
			case ETypeKind::CLASS:
			{
				delete static_cast<typename type_kind_map<ETypeKind::CLASS>::type*>(meta);
				break;
			}
			/*
			case ETypeKind::NON_UNION:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::NON_UNION>::type*>(meta);
				delete type_meta;
				break;
			}
			case ETypeKind::UNION:
			{
				auto type_meta = static_cast<typename type_kind_map<ETypeKind::UNION>::type*>(meta);
				delete type_meta;
				break;
			}
			*/
			case ETypeKind::UNSUPPORTED:
			{
				delete static_cast<typename type_kind_map<ETypeKind::UNSUPPORTED>::type*>(meta);
				break;
			}
            /*
            case ETypeKind::NON_UNION:
                delete static_cast<typename type_kind_map<ETypeKind::NON_UNION>::type*>(meta);
                break;
            case ETypeKind::UNION:
                delete static_cast<typename type_kind_map<ETypeKind::UNION>::type*>(meta);
                break;
            */
        }
	}

	static inline void declaration_delete(Declaration* decl)
	{
		using namespace ::lux::cxx::lan_model;

		delete[] decl->name;
		delete[] decl->attribute;

		switch (decl->declaration_kind)
		{
			case EDeclarationKind::CLASS:
			{
				auto declaration = static_cast<ClassDeclaration*>(decl);
                delete[] declaration->base_class_decls;
                delete[] declaration->constructor_decls;
                // delete[] declaration->destructor_decl; this pointer is saved in _unmarkable_decl_list
                delete[] declaration->field_decls; 
                delete[] declaration->member_function_decls;
                delete[] declaration->static_member_function_decls;
                delete declaration;
				break;
			}
			case EDeclarationKind::ENUMERATION:
			{
				auto declaration = static_cast<EnumerationDeclaration*>(decl);
				for (size_t i = 0; i < declaration->enumerators_num; i++)
				{
					delete declaration->enumerators[i]->name;
					delete declaration->enumerators[i];
				}
                delete[] declaration->enumerators;
				delete declaration;
				break;
			}
			case EDeclarationKind::FUNCTION:
			{
				auto declaration = static_cast<FunctionDeclaration*>(decl);
                delete[] declaration->parameter_decls;
                delete declaration;
				break;
			}
			case EDeclarationKind::PARAMETER:
			{
				delete decl;
				break;
			}
			case EDeclarationKind::MEMBER_DATA:
			{
				delete static_cast<FieldDeclaration*>(decl);
				break;
			}
			case EDeclarationKind::MEMBER_FUNCTION:
			{
				auto declaration = static_cast<MemberFunctionDeclaration*>(decl);
                delete[] declaration->parameter_decls;
                delete declaration;
				break;
			}
			case EDeclarationKind::CONSTRUCTOR:
			{
				auto declaration = static_cast<ConstructorDeclaration*>(decl);
                delete[] declaration->parameter_decls;
                delete declaration;
				break;
			}
			case EDeclarationKind::DESTRUCTOR:
			{
				auto declaration = static_cast<DestructorDeclaration*>(decl);
                delete[] declaration->parameter_decls;
                delete declaration;
				break;
			}
            case EDeclarationKind::BASIC:
            case EDeclarationKind::UNKNOWN:
                delete decl;
                break;
        }
	}

	class MetaUnitImpl
	{
	public:
		friend class MetaUnit; // parse time 

		MetaUnitImpl(std::unique_ptr<MetaUnitData> data, std::string unit_name, std::string unit_version)
			: _data(std::move(data)), _name(std::move(unit_name)), _version(std::move(unit_version))
		{
			std::string id_str = _name + _version;
			_id = hash(id_str);
		}

		void setDeleteFlat(bool flag)
		{
			_delete_flat = flag;
		}

		~MetaUnitImpl()
		{
			if (_delete_flat)
			{
				for (TypeMeta* meta : _data->_meta_type_list)
					type_delete(meta);
				for (Declaration* decl : _data->_unmarkable_decl_list)
					declaration_delete(decl);
				for (Declaration* decl : _data->_markable_decl_list)
					declaration_delete(decl);
			}
		}

	private:
		bool							_delete_flat{false};
		size_t							_id;
		std::unique_ptr<MetaUnitData>	_data;
		std::string						_name;
		std::string						_version;
	};
}
