V slucaje realizaçii koda s ispoljzovanijem "Vulkan", dvižok budet vypolnätj sledujuscije vyzovy:
* "vkCmdDispatch" - dlä otnositeljno prostoj logiki obnovlenija mira každyj kadr - vrode sveta ili obscej logiki blokov.
* "vkCmdDispatchIndirect" - dlä obrabotki prohodov logiki igry, gde kolicestvo raboty možet menätjsä ot kadra k kadru.
* "vkCmdDispatch" - dlä otsecenija nevidimyh cankov po piramide vidimosti.
* "vkCmdDispatchIndirect" - dlä postrojenija mešej cankov - každyj kadr stroitsä raznoje kolicestvo, v zavisimosti ot potrebnostej.
* "vkCmdDrawIndexedIndirect" - dlä otrisovki geometrii cankov. Kolicestvo vyzovov fiksirovano - maksimaljnoje kolicestvo cankov, pri etom kolicetvo indeksov dlä nerisujemyh cankov nulevoje.
* "vkCmdDrawIndexedIndirect" - dlä risovanija tradiçionnyh modelej. Kolicestvo vyzovov - po kolicestvu tipov modelej. Ispoljzujetsä instansing.

V çelom glavnaja problema v tom, cto kolicestvo vyzovov rasscötov ("Compute shader") i otrisovki neljzä generirovatj na storone "GPU".
Problemu možno obojti, zapuskaja vsegda fiksirovannoje kolicestvo rasscötov, no delatj nekotoryje iz nih pustymi (nulevoj razmer rabocej gruppy, 0 treugoljnikov v modeli i t. d.).
