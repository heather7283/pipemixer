# Pipewire findings
**ABANDON HOPE, ALL YE WHO ENTER HERE**
This is an attempt at documenting my pipewire findings, as it is literally
impossible to find any information on how to do things in pipewire online.
Hopefully this document will help me wrap my head around this mess, and maybe
save other innocent souls from venturing into the depths of pipewire hell.

Please note that this is by no means official documentation - I don't even know
if things I write here are correct. This is based only on empirical evidence
obtained from hours of staring at pw-dump output.


## props and params
Each PipeWire object contains a `pw_ObjectType_info` struct, which itself
contains 2 types of object that hold various information: _props_ and _params_.

### props
There is always 1 instance of _props_ per object. It is a dictionary with
string keys. That usually look like "foo.bar.baz".
```json
{
  "props": {
    "foo.bar.baz": "string",
    "meow.bark": 123,
    "aboba.amogus": true
  }
}
```
From here, I will refer to object's _prop_ as `Object."key"`, for example
`Node."foo.bar.baz"`.


### params
_params_ are objects of a certain type, like Route or Profile. There can be
several _params_ of the same type attached to a pipewire object. They are
organised as:
```json
{
  "params": {
    "ParamTypeA": [ { ... }, { ... }, ... ],
    "ParamTypeB": [ { ... }, { ... }, ... ],
    ...
  }
}
```
From here, I will refer to object's _param_ as `Object:ParamType.key`,
for example `Device:Route.index`

### Putting it together
So, a typical PipeWire object will look roughly like this:
```json
{
  "id": 96,
  "type": "PipeWire:Interface:Node",
  "info": {
    "params": {
      "ParamTypeA": [
        {
          "key": "value",
          ...
        },
        {
          "key": "value",
          ...
        },
        ...
      ],
      "ParamTypeB": [
        {
          "key": "value",
          ...
        },
        ...
      ],
      ...
    },
    "props": {
      "key": "value",
      ...
    },
    ...
  },
  ...
}
```


## Node
Pipewire docs say:

A node is a media processing element. It consumes and/or produces buffers that
contain data, such as audio or video.
A node may operate entirely inside the PipeWire daemon or it may be operating
in a client process. In the second case, media is transferred to/from that
client using the PipeWire protocol.
In an analogy to GStreamer, a node is similar (but not equal) to a GStreamer
element.

I noticed that there are 2 types of nodes - with and without a backing Device.
Let's call them D+Nodes (D-positive) and D-Nodes (D-Negative) nodes.
D+Nodes have a `device.id` field in their _props_, which contains ID of the
associated Device. D+Nodes also have a `card.profile.device` field
that contains an integer that will be useful later.

### How to get/set volume of a Node
Per-channel volume information (along with mute status, etc) is stored in a
_param_ of type Props (not to be confused with aforementioned object _props_.
They are completely different things!)

In json, it would look like this:
```json
{
  "id": 96,
  "type": "PipeWire:Interface:Node",
  "info": {
    "params": {
      "Props": [
        {
          "channelMap": [ "FL", "FR" ],
          "channelVolumes": [ 1.0, 1.0 ],
          "mute": false,
          ...
        },
        ...
      ],
      ...
    },
    "props": { ... }, // NOT THE SAME AS Props ABOVE !!!
    ...
  },
  ...
}
```
Note that in real world volume values are cubed - you need to take a cubic root
to get actual volume in percentage as you are probably used to.

To set volume/mute/whatever, you use the set-param request. Using pw-cli:
```sh
pw-cli set-param NODE_ID Props '{"channelVolumes": [0.042875, 0.027]}'
```
Remember to cube the desired percentage (30% -> 0.3 -> 0.3 ^ 3 -> 0.027).

HOWEVER. This only work on D-Nodes (see above). With D+Nodes, things are much
more complicated. (Well, you can try doing the above thing on D+Nodes, but
the results will be very weird - see for yourself if you want.)

### How to get/set volume of a D+Node
1. Find Device associated with Node (`Node."device.id" == Device.id`)
2. Get the value of `Node."card.profile.device"` (an integer)
3. Find Route _param_ where `Device:Route.device == Node."card.profile.device"`
4. Construct new Route _param_ as follows:
```json
"Route": [
  {
    "device": Route.device, // Substitute this
    "index": Route.index,   // Substitute this
    "save": true            // Not sure if this is needed
    "props": {              // Our familiar Props
      "channelVolumes": [0.042875, 0.027],
      ...
    },
  }
]
```
5.. Use it in set-param request on Device.

Say, you want to change volume of a D+Node with id 62:
```sh
# Find id of a Device associated with a D+Node with id 62
pw-dump | jq '.[] | select(.id == 62).info.props."device.id"'
# 53

# Remember Node."card.profile.device" prop value
pw-dump | jq '.[] | select(.id == 62).info.props."card.profile.device"'
# 0

# Find value of index field of a Route where Route.device == 0
pw-dump | jq '.[] | select(.id == 53).info.params.Route[] | select(.device == 0).index'
# 0

# Finally, construct new Route
route='{
  "device": 0,
  "index": 0,
  "save": true,
  "props": {
    "channelVolumes": [0.042875, 0.027]
  }
}'
pw-cli set-param 53 "$route"
```

## Device
Pipewire docs say:

A device represents a handle to an underlying API that is used to create higher
level objects, such as nodes, or other devices.
A device may have a profile, which allows the user to choose between multiple
configurations that the device may be capable of having, or to simply turn the
device off, which means that the handle is closed and not used by PipeWire.

### How to switch port on a Device (e.g. Headphones/Speakers)



