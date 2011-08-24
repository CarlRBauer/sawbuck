// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Definitions for call trace related objects common to the service and
// client libraries.

#include "syzygy/call_trace/call_trace_defs.h"

// {06255E36-14B0-4e57-8964-2E3D675A0E77}
const GUID kCallTraceProvider = {
    0x6255e36, 0x14b0, 0x4e57,
        { 0x89, 0x64, 0x2e, 0x3d, 0x67, 0x5a, 0xe, 0x77 } };

// {44CAEED0-5432-4c2d-96FA-CEC50C742F01}
const GUID kCallTraceEventClass = {
    0x44caeed0, 0x5432, 0x4c2d,
        { 0x96, 0xfa, 0xce, 0xc5, 0xc, 0x74, 0x2f, 0x1 } };

// RPC protocol and endpoint.
const wchar_t* const kCallTraceRpcProtocol = L"ncalrpc";
const wchar_t* const kCallTraceRpcEndpoint = L"syzygy-call-trace-svc";

bool CanAllocate(TraceFileSegment* segment, size_t num_bytes) {
  DCHECK(segment != NULL);
  DCHECK(segment->write_ptr != NULL);
  DCHECK(segment->end_ptr != NULL);
  DCHECK(num_bytes != 0);
  return (segment->write_ptr + num_bytes) <= segment->end_ptr;
}

void WriteSegmentHeader(SessionHandle session_handle,
                        TraceFileSegment* segment) {
  DCHECK(segment != NULL);
  DCHECK(segment->header == NULL);
  DCHECK(segment->write_ptr != NULL);
  DCHECK(CanAllocate(segment,
                     sizeof(RecordPrefix) + sizeof(TraceFileSegment::Header)));

  LARGE_INTEGER frequency = {};
  BOOL result = ::QueryPerformanceFrequency(&frequency);
  CHECK(result);

  // The trace record allocation will write the record prefix and update
  // the number of bytes consumed within the buffer. For that to work,
  // we need to make sure that the segment header is defined and minimally
  // valid (the segment_length is 0).

  segment->header = reinterpret_cast<TraceFileSegment::Header*>(
      segment->write_ptr + sizeof(RecordPrefix));
  ZeroMemory(segment->header, sizeof(*segment->header));
  segment->header->thread_handle = ::GetCurrentThreadId();
  segment->header->timer_resolution = frequency.QuadPart;

  // Let the allocator take care of writing the prefix and updating the
  // segment length.
  TraceFileSegment::Header* allocated_header =
      AllocateTraceRecord<TraceFileSegment::Header>(segment);

  // Validate that the allocator placed the header where we expected it
  // to end up.
  DCHECK(segment->header == allocated_header);
}

void* AllocateTraceRecordImpl(TraceFileSegment* segment,
                              int record_type,
                              size_t record_size) {
  DCHECK(segment != NULL);
  DCHECK(segment->header != NULL);
  DCHECK(segment->write_ptr != NULL);

  size_t total_size = sizeof(RecordPrefix) + record_size;
  DCHECK(CanAllocate(segment, total_size));

  LARGE_INTEGER timestamp = {};
  BOOL result = ::QueryPerformanceCounter(&timestamp);
  CHECK(result);

  RecordPrefix* prefix = reinterpret_cast<RecordPrefix*>(segment->write_ptr);

  prefix->size = record_size;
  prefix->version.hi = TRACE_VERSION_HI;
  prefix->version.lo = TRACE_VERSION_LO;
  prefix->type = static_cast<uint16>(record_type);
  prefix->timestamp = timestamp.QuadPart;

  segment->write_ptr += total_size;
  segment->header->segment_length += total_size;

  return prefix + 1;
}
