#ifndef __FIBRE_INTROSPECTION_HPP
#define __FIBRE_INTROSPECTION_HPP

#include <stdlib.h>
#include <algorithm>
#include <cstring>

class TypeInfo;
class Introspectable;

struct PropertyInfo {
    const char * name;
    void(*getter)(Introspectable&);
    const TypeInfo* type_info;
};

/**
 * @brief Contains runtime accessible type information.
 * 
 * Specifically, this information consists of a list of PropertyInfo items which
 * enable accessing attributes of an object by a runtime string.
 * 
 * Typically, for each combination of C++ type and Fibre interface implemented
 * by this type, one (static constant) TypeInfo object will exist.
 */
class TypeInfo {
    friend class Introspectable;
public:
    TypeInfo(const PropertyInfo* property_table, size_t property_table_length)
        : property_table_(property_table), property_table_length_(property_table_length) {}

    const PropertyInfo* get_property_info(const char * name, size_t length) const {
        for (const PropertyInfo* prop = property_table_; prop < (property_table_ + property_table_length_); ++prop) {
            if (!strncmp(name, prop->name, length)) {
                return prop;
            }
        }
        return nullptr;
    }

protected:
    template<typename T> static T& as(Introspectable& obj);
    template<typename T> static const T& as(const Introspectable& obj);
    template<typename T> static Introspectable make_introspectable(T obj, const TypeInfo* type_info);

private:
    virtual bool get_string(const Introspectable& obj, char* buffer, size_t length) const { return false; }
    virtual bool set_string(const Introspectable& obj, char* buffer, size_t length) const { return false; }

    const PropertyInfo* property_table_;
    size_t property_table_length_;
};

/**
 * @brief Wraps a reference to an application object by attaching runtime
 * accessible type information.
 * 
 * The reference that is wrapped is typically a pointer but can also be a small
 * temporary, on-demand constructed object such as a fibre::Property<...> which
 * contains multiple pointers.
 */
class Introspectable {
    friend class TypeInfo;
public:
    /**
     * @brief Returns an Introspectable object for the attribute referenced by
     * the specified attribute name.
     * 
     * The name can consist of multiple parts separated by dots.
     * 
     * If the attribute does not exist, an invalid Introspectable is returned.
     * 
     * @param path: The name or path of the attribute.
     * @param length: The maximum length of the name.
     */
    Introspectable get_child(const char * path, size_t length) {
        Introspectable current = *this;

        const char * begin = path;
        const char * end = std::find(begin, path + length, '\0');

        while ((begin < end) && current.type_info_) {
            const char * end_of_token = std::find(begin, end, '.');
            const PropertyInfo* prop_info = current.type_info_->get_property_info(begin, end_of_token - begin);
            if (prop_info) {
                (*prop_info->getter)(current);
                current.type_info_ = prop_info->type_info;
            } else {
                current.type_info_ = nullptr;
            }
            begin = std::min(end, end_of_token + 1);
        }

        return current;
    };

    bool is_valid() {
        return type_info_;
    }

    /**
     * @brief Returns the underlying value as a string. This will only succeed
     * if this Introspectable contains a Property<...> object.
     */
    bool get_string(char* buffer, size_t length) {
        return type_info_ && type_info_->get_string(*this, buffer, length);
    }

    /**
     * @brief Sets the underlying value from a string. This will only succeed
     * if this Introspectable contains a Property<...> object.
     */
    bool set_string(char* buffer, size_t length) {
        return type_info_ && type_info_->set_string(*this, buffer, length);
    }

private:
    Introspectable() {}

    // We use this storage to hold generic small objects. Usually that's a pointer
    // but sometimes it's an on-demand constructed Property<...>.
    // Caution: only put objects in here which are trivially copyable, movable
    // and destructible as any custom operation wouldn't be called.
    unsigned char storage_[12];
    const TypeInfo* type_info_ = nullptr;
};



template<typename T> T& TypeInfo::as(Introspectable& obj) {
    static_assert(sizeof(T) <= sizeof(obj.storage_));
    return *(T*)obj.storage_;
}
template<typename T> const T& TypeInfo::as(const Introspectable& obj) {
    static_assert(sizeof(T) <= sizeof(obj.storage_));
    return *(const T*)obj.storage_;
}
template<typename T> Introspectable TypeInfo::make_introspectable(T obj, const TypeInfo* type_info) {
    Introspectable introspectable;
    as<T>(introspectable) = obj;
    introspectable.type_info_ = type_info;
    return introspectable;
}


// maybe_underlying_type_t<T> resolves to the underlying type of T if T is an enum type or otherwise to T itself.
template<typename T, bool = std::is_enum<T>::value> struct maybe_underlying_type;
template<typename T> struct maybe_underlying_type<T, true> { typedef std::underlying_type_t<T> type; };
template<typename T> struct maybe_underlying_type<T, false> { typedef T type; };
template<typename T> using maybe_underlying_type_t = typename maybe_underlying_type<T>::type;



/* Built-in type infos ********************************************************/

template<typename T>
struct FibrePropertyTypeInfo;

// readonly property
template<typename T>
struct FibrePropertyTypeInfo<Property<const T>> : TypeInfo {
    using TypeInfo::TypeInfo;
    static const PropertyInfo property_table[];
    static const FibrePropertyTypeInfo<Property<const T>> singleton;

    bool get_string(const Introspectable& obj, char* buffer, size_t length) const override {
        return to_string(static_cast<maybe_underlying_type_t<T>>(as<const Property<const T>>(obj).read()), buffer, length, 0);
    }
};

template<typename T>
const PropertyInfo FibrePropertyTypeInfo<Property<const T>>::property_table[] = {};
template<typename T>
const FibrePropertyTypeInfo<Property<const T>> FibrePropertyTypeInfo<Property<const T>>::singleton{FibrePropertyTypeInfo<Property<const T>>::property_table, sizeof(FibrePropertyTypeInfo<Property<const T>>::property_table) / sizeof(FibrePropertyTypeInfo<Property<const T>>::property_table[0])};

// readwrite property
template<typename T>
struct FibrePropertyTypeInfo<Property<T>> : TypeInfo {
    using TypeInfo::TypeInfo;
    static const PropertyInfo property_table[];
    static const FibrePropertyTypeInfo<Property<T>> singleton;

    bool get_string(const Introspectable& obj, char* buffer, size_t length) const override {
        return to_string(static_cast<maybe_underlying_type_t<T>>(as<const Property<T>>(obj).read()), buffer, length, 0);
    }

    bool set_string(const Introspectable& obj, char* buffer, size_t length) const override {
        maybe_underlying_type_t<T> value;
        if (!from_string(buffer, length, &value, 0)) {
            return false;
        }
        as<const Property<T>>(obj).exchange(static_cast<T>(value));
        return true;
    }
};

template<typename T>
const PropertyInfo FibrePropertyTypeInfo<Property<T>>::property_table[] = {};
template<typename T>
const FibrePropertyTypeInfo<Property<T>> FibrePropertyTypeInfo<Property<T>>::singleton{FibrePropertyTypeInfo<Property<T>>::property_table, sizeof(FibrePropertyTypeInfo<Property<T>>::property_table) / sizeof(FibrePropertyTypeInfo<Property<T>>::property_table[0])};

#endif // __FIBRE_INTROSPECTION_HPP