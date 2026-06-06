# AkRender Shader Manager

```mermid
flowchart TD
    m[Manifest file]
    n[Shader Manager Generator Template]
    n1[Shader Manager Generator]
    m --> n1
    n --> n1
    n1 --Execute--> n2[Shader Set Config]
    n3[Shader Set runtime]
    n4[Shader Set library]
    n2 --Link--> n4
    n3 --Link--> n4
```