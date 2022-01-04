// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/test/column/binary_column_test.cpp

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "column/binary_column.h"

#include <gtest/gtest.h>

#include "column/column_helper.h"
#include "column/const_column.h"
#include "column/fixed_length_column.h"
#include "column/nullable_column.h"
#include "testutil/parallel_test.h"

namespace starrocks::vectorized {

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_create) {
    auto column = BinaryColumn::create();
    ASSERT_TRUE(column->is_binary());
    ASSERT_FALSE(column->is_nullable());
    ASSERT_EQ(0u, column->size());
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_get_data) {
    auto column = BinaryColumn::create();
    for (int i = 0; i < 100; i++) {
        column->append(std::string("str:").append(std::to_string(i)));
    }
    auto& slices = column->get_data();
    for (int i = 0; i < slices.size(); ++i) {
        ASSERT_EQ(std::string("str:").append(std::to_string(i)), slices[i].to_string());
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_byte_size) {
    auto column = BinaryColumn::create();
    ASSERT_EQ(sizeof(BinaryColumn::Offset), column->byte_size());
    std::string s("test_string");
    for (int i = 0; i < 10; i++) {
        column->append(s);
    }
    ASSERT_EQ(10, column->size());
    ASSERT_EQ(10 * s.size() + 11 * sizeof(BinaryColumn::Offset), column->byte_size());
    //                            ^^^ one more element in offset array.
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_filter) {
    auto column = BinaryColumn::create();
    for (int i = 0; i < 100; ++i) {
        column->append(std::to_string(i));
    }

    Column::Filter filter;
    for (int k = 0; k < 100; ++k) {
        filter.push_back(k % 2);
    }

    column->filter(filter);
    ASSERT_EQ(50, column->size());

    std::vector<Slice>& slices = column->get_data();

    for (int i = 0; i < 50; ++i) {
        ASSERT_EQ(std::to_string(i * 2 + 1), slices[i].to_string());
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_append_strings) {
    std::vector<Slice> values{{"hello"}, {"starrocks"}};
    auto c1 = BinaryColumn::create();
    ASSERT_TRUE(c1->append_strings(values));
    ASSERT_EQ(values.size(), c1->size());
    for (size_t i = 0; i < values.size(); i++) {
        ASSERT_EQ(values[i], c1->get_data()[i]);
    }

    // Nullable BinaryColumn
    auto c2 = NullableColumn::create(BinaryColumn::create(), NullColumn::create());
    ASSERT_TRUE(c2->append_strings(values));
    ASSERT_EQ(values.size(), c2->size());
    auto* c = reinterpret_cast<BinaryColumn*>(c2->mutable_data_column());
    for (size_t i = 0; i < values.size(); i++) {
        ASSERT_EQ(values[i], c->get_data()[i]);
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_append_numbers) {
    std::vector<int32_t> values{1, 2, 3, 4, 5};
    void* buff = values.data();
    size_t length = values.size() * sizeof(values[0]);

    auto c4 = BinaryColumn::create();
    ASSERT_EQ(-1, c4->append_numbers(buff, length));
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_append_nulls) {
    // BinaryColumn
    auto c1 = BinaryColumn::create();
    ASSERT_FALSE(c1->append_nulls(10));

    // NullableColumn
    auto c2 = NullableColumn::create(BinaryColumn::create(), NullColumn::create());
    ASSERT_TRUE(c2->append_nulls(10));
    ASSERT_EQ(10U, c2->size());
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(c2->is_null(i));
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_append_defaults) {
    // BinaryColumn
    auto c1 = BinaryColumn::create();
    c1->append_default(10);
    ASSERT_EQ(10U, c1->size());
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ("", c1->get_data()[i]);
    }

    // NullableColumn
    auto c2 = NullableColumn::create(BinaryColumn::create(), NullColumn::create());
    c2->append_default(10);
    ASSERT_EQ(10U, c2->size());
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(c2->is_null(i));
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_compare_at) {
    // Binary columns
    std::vector<Slice> strings{{"bbb"}, {"bbc"}, {"ccc"}};
    auto c1 = BinaryColumn::create();
    auto c2 = BinaryColumn::create();
    c1->append_strings(strings);
    c2->append_strings(strings);
    for (size_t i = 0; i < strings.size(); i++) {
        ASSERT_EQ(0, c1->compare_at(i, i, *c2, -1));
        ASSERT_EQ(0, c2->compare_at(i, i, *c1, -1));
    }
    for (size_t i = 0; i < strings.size(); i++) {
        for (size_t j = i + 1; j < strings.size(); j++) {
            ASSERT_LT(c1->compare_at(i, j, *c2, -1), 0);
            ASSERT_GT(c2->compare_at(j, i, *c1, -1), 0);
        }
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_append_binary) {
    auto c1 = BinaryColumn::create();
    auto c2 = BinaryColumn::create();
    c1->append(Slice("first"));

    c2->append(Slice("second"));
    c2->append(Slice("third"));

    c1->append(*c2, 0, 0);
    EXPECT_EQ(1u, c1->size());
    EXPECT_EQ("first", c1->get_slice(0));

    c1->append(*c2, 0, 2);
    EXPECT_EQ(3u, c1->size());
    EXPECT_EQ("first", c1->get_slice(0));
    EXPECT_EQ("second", c1->get_slice(1));
    EXPECT_EQ("third", c1->get_slice(2));

    c1->append(*c2, 1, 1);
    EXPECT_EQ(4u, c1->size());
    EXPECT_EQ("first", c1->get_slice(0));
    EXPECT_EQ("second", c1->get_slice(1));
    EXPECT_EQ("third", c1->get_slice(2));
    EXPECT_EQ("third", c1->get_slice(3));
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_filter_range) {
    auto column = BinaryColumn::create();

    column->append(Slice("m"));

    for (size_t i = 0; i < 63; i++) {
        column->append(Slice("a"));
    }
    column->append(Slice("bbbbbfffff"));
    column->append(Slice("c"));

    Buffer<uint8_t> filter;
    filter.emplace_back(0);
    for (size_t i = 0; i < 63; i++) {
        filter.emplace_back(1);
    }
    filter.emplace_back(1);
    filter.emplace_back(1);

    column->filter_range(filter, 0, 66);

    auto* binary_column = ColumnHelper::as_raw_column<BinaryColumn>(column);
    auto& data = binary_column->get_data();
    for (size_t i = 0; i < 63; i++) {
        ASSERT_EQ(data[i], "a");
    }
    ASSERT_EQ(data[63], "bbbbbfffff");
    ASSERT_EQ(data[64], "c");
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_resize) {
    auto c = BinaryColumn::create();
    c->append(Slice("abc"));
    c->append(Slice("def"));
    c->append(Slice("xyz"));
    c->resize(1);
    ASSERT_EQ(1u, c->size());
    ASSERT_EQ("abc", c->get_slice(0));
    c->append(Slice("xxxxxx"));
    ASSERT_EQ(2u, c->size());
    ASSERT_EQ("abc", c->get_slice(0));
    ASSERT_EQ("xxxxxx", c->get_slice(1));
    c->resize(4);
    ASSERT_EQ(4u, c->size());
    ASSERT_EQ("abc", c->get_slice(0));
    ASSERT_EQ("xxxxxx", c->get_slice(1));
    ASSERT_EQ("", c->get_slice(2));
    ASSERT_EQ("", c->get_slice(3));
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_assign) {
    std::vector<Slice> strings{{"bbb"}, {"bbc"}, {"ccc"}};
    auto c1 = BinaryColumn::create();
    auto c2 = BinaryColumn::create();
    c1->append_strings(strings);
    c2->append_strings(strings);

    c1->assign(c1->size(), 0);
    for (size_t i = 0; i < strings.size(); i++) {
        ASSERT_EQ(c1->get_slice(i), strings[0]);
    }

    c2->assign(c2->size(), 2);
    for (size_t i = 0; i < strings.size(); i++) {
        ASSERT_EQ(c2->get_slice(i), strings[2]);
    }
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_reset_column) {
    std::vector<Slice> strings{{"bbb"}, {"bbc"}, {"ccc"}};
    auto c1 = BinaryColumn::create();
    c1->append_strings(strings);
    c1->set_delete_state(DEL_PARTIAL_SATISFIED);

    c1->reset_column();
    ASSERT_EQ(0, c1->size());
    ASSERT_EQ(0, c1->get_data().size());
    ASSERT_EQ(DEL_NOT_SATISFIED, c1->delete_state());
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_swap_column) {
    std::vector<Slice> strings{{"bbb"}, {"bbc"}, {"ccc"}};
    auto c1 = BinaryColumn::create();
    c1->append_strings(strings);
    c1->set_delete_state(DEL_PARTIAL_SATISFIED);

    auto c2 = BinaryColumn::create();

    c1->swap_column(*c2);

    ASSERT_EQ(0, c1->size());
    ASSERT_EQ(0, c1->get_data().size());
    ASSERT_EQ(DEL_NOT_SATISFIED, c1->delete_state());

    ASSERT_EQ(3, c2->size());
    ASSERT_EQ(3, c2->get_data().size());
    ASSERT_EQ(DEL_PARTIAL_SATISFIED, c2->delete_state());
    ASSERT_EQ("bbb", c2->get_slice(0));
    ASSERT_EQ("bbc", c2->get_slice(1));
    ASSERT_EQ("ccc", c2->get_slice(2));
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_slice_cache) {
    auto c1 = BinaryColumn::create();
    c1->get_data().reserve(10);
    c1->append_default();
    ASSERT_FALSE(c1->_slices_cache);
    ASSERT_EQ(c1->get_offset().size(), 2);

    auto c2 = BinaryColumn::create();
    c2->get_data().reserve(10);
    c2->append_default(5);
    ASSERT_FALSE(c2->_slices_cache);
    ASSERT_EQ(c2->get_offset().size(), 6);

    auto c3 = BinaryColumn::create();
    c3->get_data().reserve(10);
    c3->append(Slice("1"));
    ASSERT_FALSE(c3->_slices_cache);
    ASSERT_EQ(c3->get_offset().size(), 2);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_reserve) {
    auto c1 = BinaryColumn::create();
    c1->reserve(10, 40);

    ASSERT_FALSE(c1->_slices_cache);
    ASSERT_EQ(c1->_offsets.capacity(), 11);
    ASSERT_EQ(c1->_bytes.capacity(), 40);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_copy_constructor) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    auto c2(*c1);

    c1.reset();
    slices = c2.get_data();
    ASSERT_EQ(2, c2.size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_move_constructor) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    auto c2(std::move(*c1));

    c1.reset();
    slices = c2.get_data();
    ASSERT_EQ(2, c2.size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_copy_assignment) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    BinaryColumn c2;
    c2 = *c1;

    c1.reset();
    slices = c2.get_data();
    ASSERT_EQ(2, c2.size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_move_assignment) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    BinaryColumn c2;
    c2 = std::move(*c1);

    c1.reset();
    slices = c2.get_data();
    ASSERT_EQ(2, c2.size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_clone) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    auto c2 = c1->clone();

    c1.reset();

    slices = down_cast<BinaryColumn*>(c2.get())->get_data();
    ASSERT_EQ(2, c2->size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_clone_shared) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    auto c2 = c1->clone_shared();
    ASSERT_TRUE(c2.unique());

    c1.reset();

    slices = down_cast<BinaryColumn*>(c2.get())->get_data();
    ASSERT_EQ(2, c2->size());
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);
}

// NOLINTNEXTLINE
PARALLEL_TEST(BinaryColumnTest, test_clone_empty) {
    auto c1 = BinaryColumn::create();
    c1->append_datum("abc");
    c1->append_datum("def");

    // trigger cache building.
    auto slices = c1->get_data();
    ASSERT_EQ("abc", slices[0]);
    ASSERT_EQ("def", slices[1]);

    auto c2 = c1->clone_empty();

    c1.reset();

    ASSERT_EQ(0, c2->size());
}

} // namespace starrocks::vectorized
