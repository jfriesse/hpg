2 node cluster (nodeA - 192.168.1.11, nodeB - 192.168.1.12) with two distinct PDUs (192.168.1.3 - apc, 192.168.1.4 - eaton) using brocade FC switch (192.168.1.2).

Brocade template:
```
kind: FenceMethodTemplate
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Method_template_name: fc_switch_brocade
  - agent_name: fence_brocade
  - parameters:
     - ipaddr: 192.168.1.2
     - password: brocade_password
     - username: brocade_admin
```

APC template:

```
kind: FenceMethodTemplate
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Method_template_name: apc_pdu
  - agent_name: fence_apc_snmp
  - must_sucess: yes
  - parameters:
     - ipaddr: 192.168.1.3
     - password: apc_password
     - username: apc_admin
```

Eaton template:
```
kind: FenceMethodTemplate
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Method_template_name: eaton_pdu
  - agent_name: fence_eaton_snmp
  - must_sucess: yes
  - parameters:
     - ipaddr: 192.168.1.4
     - password: eaton_password
     - username: eaton_admin
     - snmp-priv-prot: AES
     - snmp-priv-passwd: eaton_snmp_passwd
     - snmp-sec-level: authPriv
     - inet4-only
```

Method for enable and disable of brocade for node A:

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_fc_off
  - template: fc_switch_brocade
  - parameters:
     - plug: 1
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_fc_on
  - template: fc_switch_brocade
  - parameters:
     - plug: 1
     - action: on
```

Method for enable and disable of brocade for node B:

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeB_fc_on
  - template: fc_switch_brocade
  - parameters:
     - plug: 2
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeB_fc_off
  - template: fc_switch_brocade
  - parameters:
     - plug: 2
     - action: on
```

Methods for on/off APC (both nodes):

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_apc_off
  - template: apc_pdu
  - parameters:
     - plug: 1
     - action: off
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeB_apc_off
  - template: apc_pdu
  - parameters:
     - plug: 2
     - action: off
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_apc_on
  - template: apc_pdu
  - parameters:
     - plug: 1
     - action: on
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeB_apc_on
  - template: apc_pdu
  - parameters:
     - plug: 2
     - action: on
```

Method for on/off eaton (both nodes)

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_eaton_off
  - template: eaton_pdu
  - parameters:
     - plug: 1
     - action: off
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_eaton_off
  - template: eaton_pdu
  - parameters:
     - plug: 2
     - action: off
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_eaton_on
  - template: eaton_pdu
  - parameters:
     - plug: 1
     - action: on
```

```
kind: FenceMethodConfig
apiVersion: 1.0
metadata:
  - version: 1.0
spec:
  - Name: NodeA_eaton_on
  - template: eaton_pdu
  - parameters:
     - plug: 2
     - action: on
```

Finally fence configs:

```
kind: ConfigMap
apiVersion: v1
metadata:
  - name: nodeA_fence_config
data:
  node_fence.properties:
    - node_name: NodeA
    - isolation: NodeA_fc_off
    - power_management: [NodeA_apc_off, NodeA_eaton_off, nodeA_eaton_on, nodeA_apc_on]
    - recovery: NodeA_fc_on
```

```
kind: ConfigMap
apiVersion: v1
metadata:
  - name: nodeA_fence_config
data:
  node_fence.properties:
    - node_name: NodeB
    - isolation: NodeB_fc_off
    - power_management: [NodeB_apc_off, NodeB_eaton_off, nodeB_eaton_on, nodeB_apc_on]
    - recovery: NodeB_fc_on
```
