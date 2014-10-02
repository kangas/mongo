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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kDefault

#include "mongo/platform/basic.h"

#include "mongo/bson/util/bson_set.h"

#include <vector>

#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/bsonobjiterator.h"
#include "mongo/bson/bsontypes.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

    using mongoutils::str::stream;

    namespace {
        struct BsonCtx {
            BsonCtx(BSONObjIterator iter1, BSONObj obj2, std::string path)
                : leftIter(iter1),
                  rightObj(obj2),
                  path(path) {}

            mongo::BSONObjIterator leftIter;
            mongo::BSONObj rightObj;
            std::string path;
        };

        const std::string joinStr(StringData left, StringData right, std::string join = ".") {
            return left.empty() ? right.toString() :
                    stream() << left << join << right;
        }
    }

    void bson_set_difference(const BSONObj& input1, const BSONObj& input2, BSONObj* output) {
        std::vector<BsonCtx> stack;
        const std::vector<BsonCtx>::size_type maxDepth = 10;

        LOG(6) << "bson_set_difference#input1: " << input1.jsonString();
        LOG(6) << "bson_set_difference#input2: " << input2.jsonString();

        // Initialize stack
        BsonCtx stackRoot(BSONObjIterator(input1),
                          BSONObj(input2.objdata()),   // ensure BSONObj on stack is unowned
                          "");
        stack.push_back(stackRoot);

        //
        // Each stack entry represents:
        //  - an iterator over a sub-level of input1
        //  - a sub-object of input2 (non-owned)
        //  - a dotted path to the sub-object
        //
        // While stack entries remain:
        //   - probe input2 for input1 key
        //     if not found, join path+field and save to diffPaths
        //   - if both are objects, recur (grow stack)
        //

        std::vector<std::string> diffPaths;

        while (! stack.empty()) {
            BsonCtx& ctx = stack.back();

            if (stack.size() > maxDepth) {
                LOG(4) << "bson_set_difference#popMaxDepth: " << ctx.path;
                stack.pop_back();
                continue;
            }
            if (! ctx.leftIter.more()) {
                LOG(4) << "bson_set_difference#pop: " << ctx.path;
                stack.pop_back();
                continue;
            }

            BSONElement leftElem = ctx.leftIter.next();
            StringData leftField = leftElem.fieldNameStringData();
            LOG(4) << "bson_set_difference#test: " << ctx.path << " / " << leftField;

            if (leftElem.isABSONObj()) {
                // Grow stack
                BsonCtx stackNext(BSONObjIterator(leftElem.Obj()),
                                  ctx.rightObj.getObjectField(leftField),
                                  joinStr(ctx.path, leftField));
                stack.push_back(stackNext);
                LOG(4) << "bson_set_difference#push: " << stackNext.path;
            }
            else if (! ctx.rightObj.hasField(leftField)) {
                // Difference found
                std::string foundDiff = joinStr(ctx.path, leftField);
                LOG(3) << "bson_set_difference#foundDiff: " << foundDiff;
                diffPaths.push_back(foundDiff);
            }
        }

        //
        // Construct response. Paths to differing values are not nested objects,
        // but are dotted strings
        //
        BSONObjBuilder builder;
        for (std::vector<std::string>::iterator it = diffPaths.begin();
                it != diffPaths.end();
                ++it) {
            LOG(3) << "bson_set_difference#appending: " << *it;
            builder.appendAs(input1.getFieldDotted(*it), *it);
        }

        BSONObj result = builder.obj();
        output->swap(result);
        return;
    }
}
