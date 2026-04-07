# License & Attribution Notes

## Purpose

Track attribution and licensing impact for any Move-native Schwung module in this project.

Fill in the relevant sections when adding a new module inspired by or porting from an external source.

---

## Source Context

Document the origin of any external material this module draws from:

- **Upstream project**: name, repo URL, license (MIT, GPL, Apache, etc.)
- **Schwung runtime**: not distributed in this repository
- **Ableton Move platform**: proprietary hardware and runtime environment

If the module is original with no upstream source, state that clearly.

## Reuse Position

Choose one of these positions and document the rationale:

**Clean-room adaptation** — behavior and musical ideas are reinterpreted; no source files, assets, or data tables are copied from upstream.

**Derived work** — portions of upstream source are adapted or modified; upstream license terms apply and must be followed.

**Direct dependency** — upstream code is compiled in or linked; upstream license determines distribution terms.

## Code Reuse Matrix

Fill in this table for each area of the module:

| Area | Approach | Notes |
|---|---|---|
| core algorithm | original / adapted / copied | describe the nature of the reuse |
| data tables or lookup data | original / adapted / copied | |
| UI or asset files | original / adapted / copied | |
| Schwung wrapper | original implementation | built against ABI headers only |
| API headers | copied interface definitions | required for build compatibility |

## Attribution Guidance

If attribution is required or recommended, document the recommended public wording here.

Example:

> Inspired by [Upstream Project]. Not affiliated with [Author or Company].

If no attribution is required, state that explicitly.

## Checklist Before Public Release

- [ ] Verify the license of every upstream source used
- [ ] Confirm copied API headers do not impose additional distribution requirements
- [ ] Add upstream links if the project is published externally
- [ ] Confirm distribution terms for third-party Schwung modules on Move
- [ ] Review any assets (samples, fonts, icons) for separate license requirements
