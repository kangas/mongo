/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/bson/util/bson_set.h"

#include "mongo/bson/bsonmisc.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/json.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/log.h"

namespace {

    using namespace mongo;

    void logResult(BSONObj& result) {
        LOG(1) << "result:" << result;
    }

    TEST(Empty, Empty) {
        // Turns up log level for duration of suite
        // logger::globalLogDomain()->setMinimumLoggedSeverity(logger::LogSeverity::Debug(2));

        BSONObj input1;
        BSONObj input2;

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Flat, Identity) {
        BSONObj input1(fromjson("{ a:1, b:2.0 }"));
        BSONObj input2(input1);

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Flat, AllIncluded) {
        BSONObj input1(fromjson("{ b:2 }"));
        BSONObj input2(fromjson("{ a:1, b:2 }"));

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Flat, AllIncludedIgnoreVal) {
        BSONObj input1(fromjson("{ b:2 }"));
        BSONObj input2(fromjson("{ a:1, b:3.0 }"));

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Flat, Diff) {
        BSONObj input1(fromjson("{ a:1, b:3 }"));
        BSONObj input2(fromjson("{ b:2, c:4 }"));

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(1, result.nFields());
        ASSERT_TRUE(result.hasField("a"));
        ASSERT_EQUALS(1, result.getIntField("a"));
    }

    TEST(Nested, Identity) {
        BSONObj input1(fromjson("{ a:1, b:{c:2, d:3} }"));
        BSONObj input2(input1);

        BSONObj result;
        bson_set_difference(input1, input2, &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Nested, AllIncluded) {
        const char input1[] = "{ b:{d:3} }";
        const char input2[] = "{ a:1, b:{c:2, d:3} }";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Nested, AllIncludedIgnoreVal) {
        const char input1[] = "{ b:{d:4} }";
        const char input2[] = "{ a:1, b:{c:2, d:3} }";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(0, result.nFields());
    }

    TEST(Nested, Diff) {
        const char input1[] = "{b:{d:3, e:4}}";
        const char input2[] = "{a:1, b:{c:2, d:3}}";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(1, result.nFields());
        BSONElement e = result.getField("b.e");
        ASSERT_TRUE(e.isNumber());
        ASSERT_EQUALS(4, e.numberInt());
    }

    TEST(Nested, DiffMultiple) {
        const char input1[] = "{b:{ bb:3, bc:4}, "
                              " d:{ spam:{ eggs:'yummy' }}, "
                              " a:1 }";

        const char input2[] = "{a:1,"
                              " b:{ ba:2, bb:3 },"
                              " c:4}";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(2, result.nFields());

        ASSERT_EQUALS(4,        result.getField("b.bc").numberInt());
        ASSERT_EQUALS("yummy",  result.getField("d.spam.eggs").str());
    }

    TEST(Nested, DiffMultiple2) {
        const char input1[] = "{b:{ bb:3, bc:4 },"
                              " d:{ spam:{ eggs:'yummy' }}}";

        const char input2[] = "{a:1,"
                              " b:{ ba:2, bb:3 },"
                              " c:4,"
                              " d:{ spam:{ ham:'not' }}}";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(2, result.nFields());

        ASSERT_EQUALS(4,        result.getField("b.bc").numberInt());
        ASSERT_EQUALS("yummy",  result.getField("d.spam.eggs").str());
    }

    TEST(Nested, DiffMultiple3) {

        const char input1[] = "{b:{ bb:3, bc:4 },"
                              " d:{ foo:9, spam:{ eggs:'yummy', ham:'not' }},"
                              " blah:{blah:{blah: 'blah'}} }";

        const char input2[] = "{a:1,"
                              " b:{ ba:2, bb:3 },"
                              " c:4,"
                              " d:{ spam:{ ham:'not' }}},";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);

        ASSERT_EQUALS(4, result.nFields());

        ASSERT_EQUALS(4,        result.getField("b.bc").numberInt());
        ASSERT_EQUALS(9,        result.getField("d.foo").numberInt());
        ASSERT_EQUALS("yummy",  result.getField("d.spam.eggs").str());
        ASSERT_EQUALS("blah",   result.getField("blah.blah.blah").str());
    }

    TEST(set_param1, nonNumeric_OK) {
        const char input1[] = "{ storage: { journaling: { verbosity: \"not a number\" } } }";
        const char input2[] = "{ \"storage\" : { "
                              "       \"verbosity\" : -1, "
                              "        journaling : { "
                              "           verbosity : -1 "
                              "    } } "
                              "}";

        BSONObj result;
        bson_set_difference(fromjson(input1), fromjson(input2), &result);
        logResult(result);
        ASSERT_EQUALS(0, result.nFields());
    }
}
