# [](https://github.com/DanteG41/zbot/compare/v0.1.8...v) (2020-02-14)



## [0.1.8](https://github.com/DanteG41/zbot/compare/v0.1.0...v0.1.8) (2020-02-14)


### Bug Fixes

* **all:** exit if root user ([3258f19](https://github.com/DanteG41/zbot/commit/3258f1971a35e883d38723113d0f87c9b2e4b810))
* **cli:** changing rights ([a285209](https://github.com/DanteG41/zbot/commit/a285209abd778d203968cf36f646d9dfe2af93f9))
* **cmake:** remove test ([60b1d69](https://github.com/DanteG41/zbot/commit/60b1d698535bc349241fec69f984a715c7cd6591))
* **cmake:** static linking with TgBot ([f191dd2](https://github.com/DanteG41/zbot/commit/f191dd242901cfa0ddc5ab50d903c140ca4db91c))
* **submodule:** via HTTPS ([6c63757](https://github.com/DanteG41/zbot/commit/6c63757a76b894ce9c29e3839309b12a8502887d))
* **zstorage:** additional chmod on create ([b222351](https://github.com/DanteG41/zbot/commit/b222351e169fcfee735d66550e00b9f1fd0de6cf))
* **zstorage:** create with 2770 perm ([141f31c](https://github.com/DanteG41/zbot/commit/141f31c9f283f537108ae56dc74b5f15adaa59fd))
* ~~**zstorage:** create with 766 perm ([e28dff5](https://github.com/DanteG41/zbot/commit/e28dff5d81bcbaeeaf4d5f97aadf4da96bdc9754))~~
* send to supergroup ([3100e6a](https://github.com/DanteG41/zbot/commit/3100e6a93a14f05afb1920e572351f2c53eeadbb))


### Features

* **daemon:** set pid file ([8dc095e](https://github.com/DanteG41/zbot/commit/8dc095e3cc786557846ab9f384f578f97ad8c316))
* ~~**ebuild:** add v.0.1.3 ([c444246](https://github.com/DanteG41/zbot/commit/c444246d439abac1ede5436941aebb93548c4e22))~~



# [0.1.0](https://github.com/DanteG41/zbot/compare/3bb8c4efa0c9e2c6609e4f494da6bba05e9aad5c...v0.1.0) (2020-02-13)


### Bug Fixes

* improve commonPattern build ([c9abb63](https://github.com/DanteG41/zbot/commit/c9abb6390fde4b9b168d8283bc3a81bd2d576a9e))
* **approximation:** duplicates in message groups ([4619f64](https://github.com/DanteG41/zbot/commit/4619f64851765b456b2edc4b3138b1971b6f27ec))
* **approximation:** fix for equal strings ([3e841a5](https://github.com/DanteG41/zbot/commit/3e841a5139c4320a1bf62d113f9159768240598a))
* **config:** use pointer for asign param ([0edc166](https://github.com/DanteG41/zbot/commit/0edc166dfc54137ed6da81ab9e16763db1a3d3c7))
* **daemon:** remove args from fork name ([bf97f26](https://github.com/DanteG41/zbot/commit/bf97f26f018a7affb084472fbc64efe7364aad34))
* **setProcName:** clean argv[0] ([8a22417](https://github.com/DanteG41/zbot/commit/8a2241724ba24cfa740a802f565319312a80b280))
* **zlogger:** multiprocess write ([83c399b](https://github.com/DanteG41/zbot/commit/83c399b680f6bb4d3e03e0bca1436e0be013296a))
* **ZMsgBox:** increment of invalid iterator ([ceaa756](https://github.com/DanteG41/zbot/commit/ceaa756e373764bd045a58049426521f78bb1fe1))


### Features

* add cmake ([0370d4f](https://github.com/DanteG41/zbot/commit/0370d4f78d3387fdc5ad23163757cd27bf312b48))
* add zlogger class ([f595bd7](https://github.com/DanteG41/zbot/commit/f595bd7be43480313aa95096716acf67de49d1dc))
* **zmsgbox:** add load() ([26256a9](https://github.com/DanteG41/zbot/commit/26256a9bed16967d93d7a05b870e23609120d0cc))
* add ZMsgBox class ([3bb8c4e](https://github.com/DanteG41/zbot/commit/3bb8c4efa0c9e2c6609e4f494da6bba05e9aad5c))
* **cli:** get message from optarg ([db8f309](https://github.com/DanteG41/zbot/commit/db8f309b4c42309cc4ea4f892ee8f00ee874efb0))
* **cli:** saving message to storage ([1450df5](https://github.com/DanteG41/zbot/commit/1450df5af1ad8a20a997c2ee6a0fa9e426568d35))
* **cli:** sending to telegram ([2858715](https://github.com/DanteG41/zbot/commit/2858715f12d337f2d2e1132862f61c35c9d8bfb3))
* **daemon:** fork name in proc/cmdline ([769db40](https://github.com/DanteG41/zbot/commit/769db40679522f50eb7fe628d3b16b1f28c268d9))
* **daemon:** send messages to telegram ([480b5d2](https://github.com/DanteG41/zbot/commit/480b5d2dbb2a8f786b6bc31b4d9da5a226b1f146))
* **misc:** add sample config ([5ed1e44](https://github.com/DanteG41/zbot/commit/5ed1e44f7fe31da7717e772e61a6f61d2ef682c1))
* **submodule:** add tgbot ([ad94aa7](https://github.com/DanteG41/zbot/commit/ad94aa741cc9359329f87e28f55891924743bf9a))
* **zbotd:** running in deamon mode ([cb9b028](https://github.com/DanteG41/zbot/commit/cb9b028ea9dd639e1871e8524d68c40f4db4af79))
* **ZMsgBox:** add erase() ([a8047c0](https://github.com/DanteG41/zbot/commit/a8047c0f6f276c1ef5cfcb381494a6772e41551f))


### Performance Improvements

* **hirschberg's alg:** fix memory leak ([1e5ba19](https://github.com/DanteG41/zbot/commit/1e5ba1990d3dc01642171a6e3215295d6da3f6dc))
* improved message grouping algorithm ([ed1b7cc](https://github.com/DanteG41/zbot/commit/ed1b7cc06835be7e7952107caf13a4e2788ad8d1))
* use locale for unicode support ([35a8676](https://github.com/DanteG41/zbot/commit/35a867694b1af4f991702431e158653131e7e14c))
* **hirschberg's alg:** more performance with the same cost for del/ins ([496e18d](https://github.com/DanteG41/zbot/commit/496e18da044667c06473c3ab0da7bfa190ba9748))
* **zlogger:** improve datetime format ([a3e75d6](https://github.com/DanteG41/zbot/commit/a3e75d6cb73f1bc399e1d168050577e5aad01ec6))



