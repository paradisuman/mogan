<TeXmacs|2.1.2>

<style|<tuple|tmdoc|chinese>>

<\body>
  <tmdoc-title|Fix incorrect word count for cjk content>

  <section|Bug metadata>

  <\description>
    <item*|Reporter>jingkaimori

    <item*|Gitee issue><hlink|I7K1UK|https://gitee.com/XmacsLabs/mogan/issues/I7K1UK>
  </description>

  <section|Description>

  By default, the first paragraph of a section is not indented when
  indentation is set by <src-var|par-first> variable, corresponding to the
  behavior when <src-var|par-no-first> is set to true. But default value of
  <src-var|par-no-first> is set to false, shown below:

  <\session|scheme|default>
    <\unfolded-io|Scheme] >
      (get-init-tree "par-no-first")
    <|unfolded-io>
      <text|false>
    </unfolded-io>
  </session>

  <subsection|How to debug>

  When default value was changed, style cache in
  <verbatim|~/.TeXmacs/system/cache> should be deleted so that changed
  default value can take effect.

  <tmdoc-copyright|2023|jingkaimori>

  <tmdoc-license|Permission is granted to copy, distribute and/or modify this
  document under the terms of the GNU Free Documentation License, Version 1.1
  or any later version published by the Free Software Foundation; with no
  Invariant Sections, with no Front-Cover Texts, and with no Back-Cover
  Texts. A copy of the license is included in the section entitled "GNU Free
  Documentation License".>
</body>

<\initial>
  <\collection>
    <associate|page-medium|paper>
  </collection>
</initial>