// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: tx_excel.proto

#include "tx_excel.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/generated_message_tctable_impl.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
namespace TX {

inline constexpr constant_item::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : vlist_{},
        _vlist_cached_byte_size_{0},
        v3list_{},
        v3list64_{},
        kvlist32_{},
        id_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        value_{0},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR constant_item::constant_item(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct constant_itemDefaultTypeInternal {
  PROTOBUF_CONSTEXPR constant_itemDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~constant_itemDefaultTypeInternal() {}
  union {
    constant_item _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 constant_itemDefaultTypeInternal _constant_item_default_instance_;
      template <typename>
PROTOBUF_CONSTEXPR tx_excels_ConstantMapEntry_DoNotUse::tx_excels_ConstantMapEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct tx_excels_ConstantMapEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR tx_excels_ConstantMapEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~tx_excels_ConstantMapEntry_DoNotUseDefaultTypeInternal() {}
  union {
    tx_excels_ConstantMapEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 tx_excels_ConstantMapEntry_DoNotUseDefaultTypeInternal _tx_excels_ConstantMapEntry_DoNotUse_default_instance_;

inline constexpr tx_excels::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : constant_map_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR tx_excels::tx_excels(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct tx_excelsDefaultTypeInternal {
  PROTOBUF_CONSTEXPR tx_excelsDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~tx_excelsDefaultTypeInternal() {}
  union {
    tx_excels _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 tx_excelsDefaultTypeInternal _tx_excels_default_instance_;
}  // namespace TX
static ::_pb::Metadata file_level_metadata_tx_5fexcel_2eproto[3];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_tx_5fexcel_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_tx_5fexcel_2eproto = nullptr;
const ::uint32_t TableStruct_tx_5fexcel_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(
    protodesc_cold) = {
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels_ConstantMapEntry_DoNotUse, _has_bits_),
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels_ConstantMapEntry_DoNotUse, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels_ConstantMapEntry_DoNotUse, key_),
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels_ConstantMapEntry_DoNotUse, value_),
    0,
    1,
    ~0u,  // no _has_bits_
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::TX::tx_excels, _impl_.constant_map_),
    ~0u,  // no _has_bits_
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.id_),
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.value_),
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.vlist_),
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.v3list_),
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.v3list64_),
    PROTOBUF_FIELD_OFFSET(::TX::constant_item, _impl_.kvlist32_),
};

static const ::_pbi::MigrationSchema
    schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
        {0, 10, -1, sizeof(::TX::tx_excels_ConstantMapEntry_DoNotUse)},
        {12, -1, -1, sizeof(::TX::tx_excels)},
        {21, -1, -1, sizeof(::TX::constant_item)},
};

static const ::_pb::Message* const file_default_instances[] = {
    &::TX::_tx_excels_ConstantMapEntry_DoNotUse_default_instance_._instance,
    &::TX::_tx_excels_default_instance_._instance,
    &::TX::_constant_item_default_instance_._instance,
};
const char descriptor_table_protodef_tx_5fexcel_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
    "\n\016tx_excel.proto\022\002TX\032\023tx_base_excel.prot"
    "o\"\210\001\n\ttx_excels\0224\n\014constant_map\030\001 \003(\0132\036."
    "TX.tx_excels.ConstantMapEntry\032E\n\020Constan"
    "tMapEntry\022\013\n\003key\030\001 \001(\t\022 \n\005value\030\002 \001(\0132\021."
    "TX.constant_item:\0028\001\"\224\001\n\rconstant_item\022\n"
    "\n\002Id\030\001 \001(\t\022\r\n\005value\030\002 \001(\005\022\r\n\005vList\030\003 \003(\003"
    "\022\033\n\006v3List\030\004 \003(\0132\013.TX.v3int32\022\035\n\010v3List6"
    "4\030\005 \003(\0132\013.TX.v3int64\022\035\n\010kvList32\030\006 \003(\0132\013"
    ".TX.kvint32B\014Z\n.;tx_excelP\000b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_tx_5fexcel_2eproto_deps[1] =
    {
        &::descriptor_table_tx_5fbase_5fexcel_2eproto,
};
static ::absl::once_flag descriptor_table_tx_5fexcel_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_tx_5fexcel_2eproto = {
    false,
    false,
    355,
    descriptor_table_protodef_tx_5fexcel_2eproto,
    "tx_excel.proto",
    &descriptor_table_tx_5fexcel_2eproto_once,
    descriptor_table_tx_5fexcel_2eproto_deps,
    1,
    3,
    schemas,
    file_default_instances,
    TableStruct_tx_5fexcel_2eproto::offsets,
    file_level_metadata_tx_5fexcel_2eproto,
    file_level_enum_descriptors_tx_5fexcel_2eproto,
    file_level_service_descriptors_tx_5fexcel_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_tx_5fexcel_2eproto_getter() {
  return &descriptor_table_tx_5fexcel_2eproto;
}
// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_tx_5fexcel_2eproto(&descriptor_table_tx_5fexcel_2eproto);
namespace TX {
// ===================================================================

tx_excels_ConstantMapEntry_DoNotUse::tx_excels_ConstantMapEntry_DoNotUse() {}
tx_excels_ConstantMapEntry_DoNotUse::tx_excels_ConstantMapEntry_DoNotUse(::google::protobuf::Arena* arena)
    : SuperType(arena) {}
::google::protobuf::Metadata tx_excels_ConstantMapEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_tx_5fexcel_2eproto_getter, &descriptor_table_tx_5fexcel_2eproto_once,
      file_level_metadata_tx_5fexcel_2eproto[0]);
}
// ===================================================================

class tx_excels::_Internal {
 public:
};

tx_excels::tx_excels(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:TX.tx_excels)
}
inline PROTOBUF_NDEBUG_INLINE tx_excels::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : constant_map_{visibility, arena, from.constant_map_},
        _cached_size_{0} {}

tx_excels::tx_excels(
    ::google::protobuf::Arena* arena,
    const tx_excels& from)
    : ::google::protobuf::Message(arena) {
  tx_excels* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:TX.tx_excels)
}
inline PROTOBUF_NDEBUG_INLINE tx_excels::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : constant_map_{visibility, arena},
        _cached_size_{0} {}

inline void tx_excels::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
tx_excels::~tx_excels() {
  // @@protoc_insertion_point(destructor:TX.tx_excels)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void tx_excels::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

PROTOBUF_NOINLINE void tx_excels::Clear() {
// @@protoc_insertion_point(message_clear_start:TX.tx_excels)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.constant_map_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* tx_excels::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 2, 33, 2> tx_excels::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    2,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_tx_excels_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // map<string, .TX.constant_item> constant_map = 1;
    {PROTOBUF_FIELD_OFFSET(tx_excels, _impl_.constant_map_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMap)},
  }}, {{
    {::_pbi::TcParser::GetMapAuxInfo<
        decltype(tx_excels()._impl_.constant_map_)>(
        1, 0, 0, 9,
        11)},
    {::_pbi::TcParser::CreateInArenaStorageCb<::TX::constant_item>},
  }}, {{
    "\14\14\0\0\0\0\0\0"
    "TX.tx_excels"
    "constant_map"
  }},
};

::uint8_t* tx_excels::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:TX.tx_excels)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // map<string, .TX.constant_item> constant_map = 1;
  if (!_internal_constant_map().empty()) {
    using MapType = ::google::protobuf::Map<std::string, ::TX::constant_item>;
    using WireHelper = _pbi::MapEntryFuncs<std::string, ::TX::constant_item,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>;
    const auto& field = _internal_constant_map();

    if (stream->IsSerializationDeterministic() && field.size() > 1) {
      for (const auto& entry : ::google::protobuf::internal::MapSorterPtr<MapType>(field)) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "TX.tx_excels.constant_map");
      }
    } else {
      for (const auto& entry : field) {
        target = WireHelper::InternalSerialize(
            1, entry.first, entry.second, target, stream);
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            entry.first.data(), static_cast<int>(entry.first.length()),
 ::google::protobuf::internal::WireFormatLite::SERIALIZE, "TX.tx_excels.constant_map");
      }
    }
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:TX.tx_excels)
  return target;
}

::size_t tx_excels::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:TX.tx_excels)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // map<string, .TX.constant_item> constant_map = 1;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_constant_map_size());
  for (const auto& entry : _internal_constant_map()) {
    total_size += _pbi::MapEntryFuncs<std::string, ::TX::constant_item,
                                   _pbi::WireFormatLite::TYPE_STRING,
                                   _pbi::WireFormatLite::TYPE_MESSAGE>::ByteSizeLong(entry.first, entry.second);
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::google::protobuf::Message::ClassData tx_excels::_class_data_ = {
    tx_excels::MergeImpl,
    nullptr,  // OnDemandRegisterArenaDtor
};
const ::google::protobuf::Message::ClassData* tx_excels::GetClassData() const {
  return &_class_data_;
}

void tx_excels::MergeImpl(::google::protobuf::Message& to_msg, const ::google::protobuf::Message& from_msg) {
  auto* const _this = static_cast<tx_excels*>(&to_msg);
  auto& from = static_cast<const tx_excels&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:TX.tx_excels)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_impl_.constant_map_.MergeFrom(from._impl_.constant_map_);
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void tx_excels::CopyFrom(const tx_excels& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:TX.tx_excels)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool tx_excels::IsInitialized() const {
  return true;
}

::_pbi::CachedSize* tx_excels::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void tx_excels::InternalSwap(tx_excels* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.constant_map_.InternalSwap(&other->_impl_.constant_map_);
}

::google::protobuf::Metadata tx_excels::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_tx_5fexcel_2eproto_getter, &descriptor_table_tx_5fexcel_2eproto_once,
      file_level_metadata_tx_5fexcel_2eproto[1]);
}
// ===================================================================

class constant_item::_Internal {
 public:
};

void constant_item::clear_v3list() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.v3list_.Clear();
}
void constant_item::clear_v3list64() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.v3list64_.Clear();
}
void constant_item::clear_kvlist32() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.kvlist32_.Clear();
}
constant_item::constant_item(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:TX.constant_item)
}
inline PROTOBUF_NDEBUG_INLINE constant_item::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : vlist_{visibility, arena, from.vlist_},
        _vlist_cached_byte_size_{0},
        v3list_{visibility, arena, from.v3list_},
        v3list64_{visibility, arena, from.v3list64_},
        kvlist32_{visibility, arena, from.kvlist32_},
        id_(arena, from.id_),
        _cached_size_{0} {}

constant_item::constant_item(
    ::google::protobuf::Arena* arena,
    const constant_item& from)
    : ::google::protobuf::Message(arena) {
  constant_item* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  _impl_.value_ = from._impl_.value_;

  // @@protoc_insertion_point(copy_constructor:TX.constant_item)
}
inline PROTOBUF_NDEBUG_INLINE constant_item::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : vlist_{visibility, arena},
        _vlist_cached_byte_size_{0},
        v3list_{visibility, arena},
        v3list64_{visibility, arena},
        kvlist32_{visibility, arena},
        id_(arena),
        _cached_size_{0} {}

inline void constant_item::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.value_ = {};
}
constant_item::~constant_item() {
  // @@protoc_insertion_point(destructor:TX.constant_item)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void constant_item::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.id_.Destroy();
  _impl_.~Impl_();
}

PROTOBUF_NOINLINE void constant_item::Clear() {
// @@protoc_insertion_point(message_clear_start:TX.constant_item)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.vlist_.Clear();
  _impl_.v3list_.Clear();
  _impl_.v3list64_.Clear();
  _impl_.kvlist32_.Clear();
  _impl_.id_.ClearToEmpty();
  _impl_.value_ = 0;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* constant_item::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<3, 6, 3, 27, 2> constant_item::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    6, 56,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967232,  // skipmap
    offsetof(decltype(_table_), field_entries),
    6,  // num_field_entries
    3,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_constant_item_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // string Id = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.id_)}},
    // int32 value = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(constant_item, _impl_.value_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.value_)}},
    // repeated int64 vList = 3;
    {::_pbi::TcParser::FastV64P1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.vlist_)}},
    // repeated .TX.v3int32 v3List = 4;
    {::_pbi::TcParser::FastMtR1,
     {34, 63, 0, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.v3list_)}},
    // repeated .TX.v3int64 v3List64 = 5;
    {::_pbi::TcParser::FastMtR1,
     {42, 63, 1, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.v3list64_)}},
    // repeated .TX.kvint32 kvList32 = 6;
    {::_pbi::TcParser::FastMtR1,
     {50, 63, 2, PROTOBUF_FIELD_OFFSET(constant_item, _impl_.kvlist32_)}},
    {::_pbi::TcParser::MiniParse, {}},
  }}, {{
    65535, 65535
  }}, {{
    // string Id = 1;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // int32 value = 2;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.value_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // repeated int64 vList = 3;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.vlist_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kPackedInt64)},
    // repeated .TX.v3int32 v3List = 4;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.v3list_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .TX.v3int64 v3List64 = 5;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.v3list64_), 0, 1,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .TX.kvint32 kvList32 = 6;
    {PROTOBUF_FIELD_OFFSET(constant_item, _impl_.kvlist32_), 0, 2,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::TX::v3int32>()},
    {::_pbi::TcParser::GetTable<::TX::v3int64>()},
    {::_pbi::TcParser::GetTable<::TX::kvint32>()},
  }}, {{
    "\20\2\0\0\0\0\0\0"
    "TX.constant_item"
    "Id"
  }},
};

::uint8_t* constant_item::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:TX.constant_item)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string Id = 1;
  if (!this->_internal_id().empty()) {
    const std::string& _s = this->_internal_id();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "TX.constant_item.Id");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // int32 value = 2;
  if (this->_internal_value() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_value(), target);
  }

  // repeated int64 vList = 3;
  {
    int byte_size = _impl_._vlist_cached_byte_size_.Get();
    if (byte_size > 0) {
      target = stream->WriteInt64Packed(
          3, _internal_vlist(), byte_size, target);
    }
  }

  // repeated .TX.v3int32 v3List = 4;
  for (unsigned i = 0,
      n = static_cast<unsigned>(this->_internal_v3list_size()); i < n; i++) {
    const auto& repfield = this->_internal_v3list().Get(i);
    target = ::google::protobuf::internal::WireFormatLite::
        InternalWriteMessage(4, repfield, repfield.GetCachedSize(), target, stream);
  }

  // repeated .TX.v3int64 v3List64 = 5;
  for (unsigned i = 0,
      n = static_cast<unsigned>(this->_internal_v3list64_size()); i < n; i++) {
    const auto& repfield = this->_internal_v3list64().Get(i);
    target = ::google::protobuf::internal::WireFormatLite::
        InternalWriteMessage(5, repfield, repfield.GetCachedSize(), target, stream);
  }

  // repeated .TX.kvint32 kvList32 = 6;
  for (unsigned i = 0,
      n = static_cast<unsigned>(this->_internal_kvlist32_size()); i < n; i++) {
    const auto& repfield = this->_internal_kvlist32().Get(i);
    target = ::google::protobuf::internal::WireFormatLite::
        InternalWriteMessage(6, repfield, repfield.GetCachedSize(), target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:TX.constant_item)
  return target;
}

::size_t constant_item::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:TX.constant_item)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated int64 vList = 3;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::Int64Size(
        this->_internal_vlist())
    ;
    _impl_._vlist_cached_byte_size_.Set(::_pbi::ToCachedSize(data_size));
    std::size_t tag_size = data_size == 0
        ? 0
        : 1 + ::_pbi::WireFormatLite::Int32Size(
                            static_cast<int32_t>(data_size))
    ;
    total_size += tag_size + data_size;
  }
  // repeated .TX.v3int32 v3List = 4;
  total_size += 1UL * this->_internal_v3list_size();
  for (const auto& msg : this->_internal_v3list()) {
    total_size +=
      ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .TX.v3int64 v3List64 = 5;
  total_size += 1UL * this->_internal_v3list64_size();
  for (const auto& msg : this->_internal_v3list64()) {
    total_size +=
      ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // repeated .TX.kvint32 kvList32 = 6;
  total_size += 1UL * this->_internal_kvlist32_size();
  for (const auto& msg : this->_internal_kvlist32()) {
    total_size +=
      ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  // string Id = 1;
  if (!this->_internal_id().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_id());
  }

  // int32 value = 2;
  if (this->_internal_value() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_value());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::google::protobuf::Message::ClassData constant_item::_class_data_ = {
    constant_item::MergeImpl,
    nullptr,  // OnDemandRegisterArenaDtor
};
const ::google::protobuf::Message::ClassData* constant_item::GetClassData() const {
  return &_class_data_;
}

void constant_item::MergeImpl(::google::protobuf::Message& to_msg, const ::google::protobuf::Message& from_msg) {
  auto* const _this = static_cast<constant_item*>(&to_msg);
  auto& from = static_cast<const constant_item&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:TX.constant_item)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_vlist()->MergeFrom(from._internal_vlist());
  _this->_internal_mutable_v3list()->MergeFrom(
      from._internal_v3list());
  _this->_internal_mutable_v3list64()->MergeFrom(
      from._internal_v3list64());
  _this->_internal_mutable_kvlist32()->MergeFrom(
      from._internal_kvlist32());
  if (!from._internal_id().empty()) {
    _this->_internal_set_id(from._internal_id());
  }
  if (from._internal_value() != 0) {
    _this->_internal_set_value(from._internal_value());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void constant_item::CopyFrom(const constant_item& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:TX.constant_item)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool constant_item::IsInitialized() const {
  return true;
}

::_pbi::CachedSize* constant_item::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void constant_item::InternalSwap(constant_item* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.vlist_.InternalSwap(&other->_impl_.vlist_);
  _impl_.v3list_.InternalSwap(&other->_impl_.v3list_);
  _impl_.v3list64_.InternalSwap(&other->_impl_.v3list64_);
  _impl_.kvlist32_.InternalSwap(&other->_impl_.kvlist32_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.id_, &other->_impl_.id_, arena);
        swap(_impl_.value_, other->_impl_.value_);
}

::google::protobuf::Metadata constant_item::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_tx_5fexcel_2eproto_getter, &descriptor_table_tx_5fexcel_2eproto_once,
      file_level_metadata_tx_5fexcel_2eproto[2]);
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace TX
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
#include "google/protobuf/port_undef.inc"