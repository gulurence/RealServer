// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: rpc/myservice.proto

#include "rpc/myservice.pb.h"

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
namespace myservice {

inline constexpr HelloRequest::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : name_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        index_{0},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR HelloRequest::HelloRequest(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct HelloRequestDefaultTypeInternal {
  PROTOBUF_CONSTEXPR HelloRequestDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~HelloRequestDefaultTypeInternal() {}
  union {
    HelloRequest _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 HelloRequestDefaultTypeInternal _HelloRequest_default_instance_;

inline constexpr HelloReply::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : message_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        index_{0},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR HelloReply::HelloReply(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct HelloReplyDefaultTypeInternal {
  PROTOBUF_CONSTEXPR HelloReplyDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~HelloReplyDefaultTypeInternal() {}
  union {
    HelloReply _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 HelloReplyDefaultTypeInternal _HelloReply_default_instance_;
}  // namespace myservice
static ::_pb::Metadata file_level_metadata_rpc_2fmyservice_2eproto[2];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_rpc_2fmyservice_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_rpc_2fmyservice_2eproto = nullptr;
const ::uint32_t TableStruct_rpc_2fmyservice_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(
    protodesc_cold) = {
    ~0u,  // no _has_bits_
    PROTOBUF_FIELD_OFFSET(::myservice::HelloRequest, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::myservice::HelloRequest, _impl_.name_),
    PROTOBUF_FIELD_OFFSET(::myservice::HelloRequest, _impl_.index_),
    ~0u,  // no _has_bits_
    PROTOBUF_FIELD_OFFSET(::myservice::HelloReply, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::myservice::HelloReply, _impl_.message_),
    PROTOBUF_FIELD_OFFSET(::myservice::HelloReply, _impl_.index_),
};

static const ::_pbi::MigrationSchema
    schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::myservice::HelloRequest)},
        {10, -1, -1, sizeof(::myservice::HelloReply)},
};

static const ::_pb::Message* const file_default_instances[] = {
    &::myservice::_HelloRequest_default_instance_._instance,
    &::myservice::_HelloReply_default_instance_._instance,
};
const char descriptor_table_protodef_rpc_2fmyservice_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
    "\n\023rpc/myservice.proto\022\tmyservice\"+\n\014Hell"
    "oRequest\022\014\n\004name\030\001 \001(\t\022\r\n\005index\030\002 \001(\005\",\n"
    "\nHelloReply\022\017\n\007message\030\001 \001(\t\022\r\n\005index\030\002 "
    "\001(\0052G\n\tMyService\022:\n\010SayHello\022\027.myservice"
    ".HelloRequest\032\025.myservice.HelloReplyb\006pr"
    "oto3"
};
static ::absl::once_flag descriptor_table_rpc_2fmyservice_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_rpc_2fmyservice_2eproto = {
    false,
    false,
    204,
    descriptor_table_protodef_rpc_2fmyservice_2eproto,
    "rpc/myservice.proto",
    &descriptor_table_rpc_2fmyservice_2eproto_once,
    nullptr,
    0,
    2,
    schemas,
    file_default_instances,
    TableStruct_rpc_2fmyservice_2eproto::offsets,
    file_level_metadata_rpc_2fmyservice_2eproto,
    file_level_enum_descriptors_rpc_2fmyservice_2eproto,
    file_level_service_descriptors_rpc_2fmyservice_2eproto,
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
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_rpc_2fmyservice_2eproto_getter() {
  return &descriptor_table_rpc_2fmyservice_2eproto;
}
// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_rpc_2fmyservice_2eproto(&descriptor_table_rpc_2fmyservice_2eproto);
namespace myservice {
// ===================================================================

class HelloRequest::_Internal {
 public:
};

HelloRequest::HelloRequest(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:myservice.HelloRequest)
}
inline PROTOBUF_NDEBUG_INLINE HelloRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : name_(arena, from.name_),
        _cached_size_{0} {}

HelloRequest::HelloRequest(
    ::google::protobuf::Arena* arena,
    const HelloRequest& from)
    : ::google::protobuf::Message(arena) {
  HelloRequest* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  _impl_.index_ = from._impl_.index_;

  // @@protoc_insertion_point(copy_constructor:myservice.HelloRequest)
}
inline PROTOBUF_NDEBUG_INLINE HelloRequest::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : name_(arena),
        _cached_size_{0} {}

inline void HelloRequest::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.index_ = {};
}
HelloRequest::~HelloRequest() {
  // @@protoc_insertion_point(destructor:myservice.HelloRequest)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void HelloRequest::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.name_.Destroy();
  _impl_.~Impl_();
}

PROTOBUF_NOINLINE void HelloRequest::Clear() {
// @@protoc_insertion_point(message_clear_start:myservice.HelloRequest)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.name_.ClearToEmpty();
  _impl_.index_ = 0;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* HelloRequest::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 35, 2> HelloRequest::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_HelloRequest_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
  }, {{
    // int32 index = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(HelloRequest, _impl_.index_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(HelloRequest, _impl_.index_)}},
    // string name = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(HelloRequest, _impl_.name_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string name = 1;
    {PROTOBUF_FIELD_OFFSET(HelloRequest, _impl_.name_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // int32 index = 2;
    {PROTOBUF_FIELD_OFFSET(HelloRequest, _impl_.index_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
  }},
  // no aux_entries
  {{
    "\26\4\0\0\0\0\0\0"
    "myservice.HelloRequest"
    "name"
  }},
};

::uint8_t* HelloRequest::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:myservice.HelloRequest)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string name = 1;
  if (!this->_internal_name().empty()) {
    const std::string& _s = this->_internal_name();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "myservice.HelloRequest.name");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // int32 index = 2;
  if (this->_internal_index() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_index(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:myservice.HelloRequest)
  return target;
}

::size_t HelloRequest::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:myservice.HelloRequest)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string name = 1;
  if (!this->_internal_name().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_name());
  }

  // int32 index = 2;
  if (this->_internal_index() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_index());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::google::protobuf::Message::ClassData HelloRequest::_class_data_ = {
    HelloRequest::MergeImpl,
    nullptr,  // OnDemandRegisterArenaDtor
};
const ::google::protobuf::Message::ClassData* HelloRequest::GetClassData() const {
  return &_class_data_;
}

void HelloRequest::MergeImpl(::google::protobuf::Message& to_msg, const ::google::protobuf::Message& from_msg) {
  auto* const _this = static_cast<HelloRequest*>(&to_msg);
  auto& from = static_cast<const HelloRequest&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:myservice.HelloRequest)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_name().empty()) {
    _this->_internal_set_name(from._internal_name());
  }
  if (from._internal_index() != 0) {
    _this->_internal_set_index(from._internal_index());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void HelloRequest::CopyFrom(const HelloRequest& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:myservice.HelloRequest)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool HelloRequest::IsInitialized() const {
  return true;
}

::_pbi::CachedSize* HelloRequest::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void HelloRequest::InternalSwap(HelloRequest* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.name_, &other->_impl_.name_, arena);
        swap(_impl_.index_, other->_impl_.index_);
}

::google::protobuf::Metadata HelloRequest::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_rpc_2fmyservice_2eproto_getter, &descriptor_table_rpc_2fmyservice_2eproto_once,
      file_level_metadata_rpc_2fmyservice_2eproto[0]);
}
// ===================================================================

class HelloReply::_Internal {
 public:
};

HelloReply::HelloReply(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:myservice.HelloReply)
}
inline PROTOBUF_NDEBUG_INLINE HelloReply::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : message_(arena, from.message_),
        _cached_size_{0} {}

HelloReply::HelloReply(
    ::google::protobuf::Arena* arena,
    const HelloReply& from)
    : ::google::protobuf::Message(arena) {
  HelloReply* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  _impl_.index_ = from._impl_.index_;

  // @@protoc_insertion_point(copy_constructor:myservice.HelloReply)
}
inline PROTOBUF_NDEBUG_INLINE HelloReply::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : message_(arena),
        _cached_size_{0} {}

inline void HelloReply::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.index_ = {};
}
HelloReply::~HelloReply() {
  // @@protoc_insertion_point(destructor:myservice.HelloReply)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void HelloReply::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.message_.Destroy();
  _impl_.~Impl_();
}

PROTOBUF_NOINLINE void HelloReply::Clear() {
// @@protoc_insertion_point(message_clear_start:myservice.HelloReply)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.message_.ClearToEmpty();
  _impl_.index_ = 0;
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* HelloReply::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<1, 2, 0, 36, 2> HelloReply::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    2, 8,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967292,  // skipmap
    offsetof(decltype(_table_), field_entries),
    2,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_HelloReply_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
  }, {{
    // int32 index = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(HelloReply, _impl_.index_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(HelloReply, _impl_.index_)}},
    // string message = 1;
    {::_pbi::TcParser::FastUS1,
     {10, 63, 0, PROTOBUF_FIELD_OFFSET(HelloReply, _impl_.message_)}},
  }}, {{
    65535, 65535
  }}, {{
    // string message = 1;
    {PROTOBUF_FIELD_OFFSET(HelloReply, _impl_.message_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kUtf8String | ::_fl::kRepAString)},
    // int32 index = 2;
    {PROTOBUF_FIELD_OFFSET(HelloReply, _impl_.index_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
  }},
  // no aux_entries
  {{
    "\24\7\0\0\0\0\0\0"
    "myservice.HelloReply"
    "message"
  }},
};

::uint8_t* HelloReply::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:myservice.HelloReply)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // string message = 1;
  if (!this->_internal_message().empty()) {
    const std::string& _s = this->_internal_message();
    ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
        _s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormatLite::SERIALIZE, "myservice.HelloReply.message");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // int32 index = 2;
  if (this->_internal_index() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_index(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:myservice.HelloReply)
  return target;
}

::size_t HelloReply::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:myservice.HelloReply)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // string message = 1;
  if (!this->_internal_message().empty()) {
    total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                    this->_internal_message());
  }

  // int32 index = 2;
  if (this->_internal_index() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_index());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}

const ::google::protobuf::Message::ClassData HelloReply::_class_data_ = {
    HelloReply::MergeImpl,
    nullptr,  // OnDemandRegisterArenaDtor
};
const ::google::protobuf::Message::ClassData* HelloReply::GetClassData() const {
  return &_class_data_;
}

void HelloReply::MergeImpl(::google::protobuf::Message& to_msg, const ::google::protobuf::Message& from_msg) {
  auto* const _this = static_cast<HelloReply*>(&to_msg);
  auto& from = static_cast<const HelloReply&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:myservice.HelloReply)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (!from._internal_message().empty()) {
    _this->_internal_set_message(from._internal_message());
  }
  if (from._internal_index() != 0) {
    _this->_internal_set_index(from._internal_index());
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void HelloReply::CopyFrom(const HelloReply& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:myservice.HelloReply)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool HelloReply::IsInitialized() const {
  return true;
}

::_pbi::CachedSize* HelloReply::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void HelloReply::InternalSwap(HelloReply* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.message_, &other->_impl_.message_, arena);
        swap(_impl_.index_, other->_impl_.index_);
}

::google::protobuf::Metadata HelloReply::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_rpc_2fmyservice_2eproto_getter, &descriptor_table_rpc_2fmyservice_2eproto_once,
      file_level_metadata_rpc_2fmyservice_2eproto[1]);
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace myservice
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
#include "google/protobuf/port_undef.inc"