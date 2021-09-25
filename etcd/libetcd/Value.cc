#include "libetcd.h"

namespace libetcd {

Value::Value(const ::etcdserverpb::KeyValue& kv)
    : key_(kv.key())
    , create_revision_(kv.create_revision())
    , mod_revision_(kv.mod_revision())
    , version_(kv.version())
    , value_(kv.value())
    , lease_(kv.lease())
    , valid_(true)
{}

}
