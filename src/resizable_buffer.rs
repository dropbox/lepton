// Copyright 2017 Dropbox, Inc
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

use alloc::{Allocator, SliceWrapper, SliceWrapperMut};
use core;

pub struct ResizableByteBuffer<T: Sized + Default, AllocT: Allocator<T>> {
    data: AllocT::AllocatedMemory,
    size: usize,
}
impl<T: Sized + Default + Clone, AllocT: Allocator<T>> Default for ResizableByteBuffer<T, AllocT> {
    fn default() -> Self {
        Self::new()
    }
}
impl<T: Sized + Default + Clone, AllocT: Allocator<T>> ResizableByteBuffer<T, AllocT> {
    pub fn new() -> Self {
        ResizableByteBuffer::<T, AllocT> {
            data: AllocT::AllocatedMemory::default(),
            size: 0,
        }
    }
    fn ensure_free_space_in_buffer(&mut self, allocator: &mut AllocT, min_size: usize) {
        if self.data.slice().is_empty() {
            self.data = allocator.alloc_cell(66_000); // some slack room to deal with worst case compression sizes
        } else if self.size + min_size > self.data.slice().len() {
            let mut cell = allocator.alloc_cell(self.size * 2);
            cell.slice_mut()
                .split_at_mut(self.size)
                .0
                .clone_from_slice(self.data.slice().split_at(self.size).0);
            allocator.free_cell(core::mem::replace(&mut self.data, cell));
        }
    }
    pub fn checkout_next_buffer(
        &mut self,
        allocator: &mut AllocT,
        min_size: Option<usize>,
    ) -> &mut [T] {
        self.ensure_free_space_in_buffer(allocator, min_size.unwrap_or(1));
        self.data.slice_mut().split_at_mut(self.size).1
    }
    pub fn commit_next_buffer(&mut self, size: usize) {
        self.size += size;
    }
    pub fn len(&self) -> usize {
        self.size
    }
    pub fn is_empty(&self) -> bool {
        self.size == 0
    }
    pub fn slice(&self) -> &[T] {
        self.data.slice().split_at(self.size).0
    }
    pub fn free(&mut self, allocator: &mut AllocT) {
        allocator.free_cell(core::mem::replace(
            &mut self.data,
            AllocT::AllocatedMemory::default(),
        ))
    }
}
