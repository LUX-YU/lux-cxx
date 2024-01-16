#pragma once
#include <type_traits>
#include "Class.hpp"
#include "Function.hpp"

namespace lux::cxx::dref
{
	struct LuxDRefObject
	{
		DataTypeMeta* type_meta;
		void* object;
	};

	static constexpr inline LuxDRefObject null_dref_object{nullptr, nullptr};

	static constexpr bool is_dref_obj_vaild(const LuxDRefObject& obj)
	{
		return obj.object == nullptr || obj.type_meta == nullptr;
	}

	class Instance
	{
	public:
		~Instance()
		{
			auto _meta = static_cast<DataTypeMeta*>(_dref_object.type_meta);
			Function destructor(_meta->destructor_meta);
			destructor.tInvoke(
				empty_func_arg,
				LUX_ARG(void*, _dref_object.object)
			);
		}

		DataTypeMeta* const metaType() const
		{
			return _dref_object.type_meta;
		}

	protected:
		Instance(const LuxDRefObject& object)
			: _dref_object(object) {}

		LuxDRefObject _dref_object;
	};

	class BasicTypeInstance : public Instance
	{
	public:
		explicit BasicTypeInstance(const LuxDRefObject& object)
			: Instance(object) {}

		template<typename T>
		T* as_ptr() requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}

		template<typename T>
		const T* const as_ptr() const requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}

		template<typename T>
		const T* const as_ptr_hard() const requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}
	};

	class ClassInstance : public Instance
	{
	private:
		template<typename... Args>
		static LuxDRefObject DreffromClassMeta(ClassMeta* const _m_class_meta, Args&&... args)
		{
			Function constructor(_m_class_meta->constructor_meta);
			void* ret_buff;
			return constructor.tInvoke(LUX_ARG(void*, ret_buff), std::forward<Args>(args)...) ?
				LuxDRefObject{ _m_class_meta, ret_buff } : null_dref_object;
		}

		template<typename... Args>
		static LuxDRefObject DreffromClass(const Class& _m_class, Args&&... args)
		{
			return DreffromClassMeta(_m_class.classMeta(), std::forward<Args>(args)...);
		}

	public:
		explicit ClassInstance(const LuxDRefObject& object)
			: Instance(object) {}

		template<typename... Args>
		ClassInstance(const Class& _m_class, Args&&... args)
			: Instance(DreffromClass(_m_class, std::forward<Args>(args)...)) {}

		template<typename... Args>
		ClassInstance(ClassMeta* const _m_class_meta, Args&&... args)
			: Instance(DreffromClassMeta(_m_class_meta, std::forward<Args>(args)...)) {}

		static ClassInstance invaildInstance()
		{
			return ClassInstance(null_dref_object);
		}

		enum class InvaildReason
		{
			SUCCESS,
			EMPTY_OBJECT,
			OBJECT_TYPE_UNMATCH,
			UNKNOWN
		};

		ClassInstance::InvaildReason isVaild() const
		{
			if (_dref_object.type_meta->meta_type != MetaType::CLASS)
				return InvaildReason::OBJECT_TYPE_UNMATCH;
			else if (!is_dref_obj_vaild(_dref_object))
				return InvaildReason::EMPTY_OBJECT;

			return InvaildReason::SUCCESS;
		}

		FieldInfo* const getFieldInfo(std::string_view name) const
		{
			auto  class_meta = classMeta();
			auto& list = class_meta->field_meta_list;
			auto  iter = std::find_if(list.begin(), list.end(),
				[name](FieldInfo& meta)
				{
					return meta.field_name == name;
				}
			);

			if (iter == list.end())
				return nullptr;

			return &*iter;
		}

		LuxDRefObject getField(std::string_view name)
		{
			auto info = getFieldInfo(name);

			if (info == nullptr)
				return null_dref_object;

			void* object_ptr = reinterpret_cast<void*>(
				reinterpret_cast<uint8_t*>(_dref_object.object) + info->offset
				);

			return LuxDRefObject{ info->type_meta, object_ptr };
		}

		template<typename T>
		T getFieldAs(std::string_view name)
			requires std::is_base_of_v<Instance, T>
		{
			return T{ getField(name) };
		}

		ClassMeta* const classMeta() const
		{
			return static_cast<ClassMeta* const>(metaType());
		}

		const std::vector<FieldInfo>& fieldInfoLists() const
		{
			return classMeta()->field_meta_list;
		}

		Method getMethod(std::string_view name)
		{
			auto  class_meta = classMeta();
			auto& list = class_meta->method_meta_list;
			auto  iter = std::find_if(list.begin(), list.end(),
				[name](MethodMeta* meta)
				{
					return meta->func_name == name;
				}
			);
			return iter == list.end() ?
				Method(nullptr, nullptr) :
				Method(*iter, _dref_object.object);
		}

		const std::vector<MethodMeta*>& methodMetaList() const
		{
			return classMeta()->method_meta_list;
		}
	};
}
