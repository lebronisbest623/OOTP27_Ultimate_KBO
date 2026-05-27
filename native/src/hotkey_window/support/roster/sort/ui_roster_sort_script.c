#include "../../text/js/ui_js_string.h"
#include "../../text/language/ui_language.h"
#include "ui_roster_sort_script.h"
#include "../../text/buffer/ui_text_buffer.h"

void kbo_webview_append_roster_sort_script_body(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    const char* confirm_title = kbo_hub_text("\xec\xa7\x88\xeb\xac\xb8", "Question");
    const char* confirm_message = kbo_hub_text(
        "\xec\xa0\x95\xeb\xa7\x90\xeb\xa1\x9c %s\xec\x9d\x98 \xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c\xec\x9d\x84 \xed\x95\xb4\xec\xa0\x9c\xed\x95\x98\xec\x8b\x9c\xea\xb2\xa0\xec\x8a\xb5\xeb\x8b\x88\xea\xb9\x8c?",
        "Do you really want to release %s?");
    const char* confirm_player = kbo_hub_text("\xec\x9d\xb4 \xec\x84\xa0\xec\x88\x98", "this player");
    const char* confirm_cancel = kbo_hub_text("\xec\xb7\xa8\xec\x86\x8c", "Cancel");
    const char* confirm_ok = kbo_hub_text("\xed\x99\x95\xec\x9d\xb8", "OK");
    kbo_window_text_appendf(
        buffer,
        "(function(){"
        "function isEditableTarget(node){while(node&&node!==document){var tag=(node.tagName||'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select'||node.isContentEditable){return true;}node=node.parentNode;}return false;}"
        "function installGameSurfaceGuards(){if(!window.__kboGameSurfaceGuardsInstalled){window.__kboGameSurfaceGuardsInstalled=1;document.addEventListener('selectstart',function(e){if(!isEditableTarget(e.target)){e.preventDefault();}},true);document.addEventListener('dragstart',function(e){e.preventDefault();},true);document.addEventListener('mousedown',function(e){if(e.detail>1&&!isEditableTarget(e.target)){e.preventDefault();}},true);}var images=document.querySelectorAll('img');for(var i=0;i<images.length;i++){images[i].setAttribute('draggable','false');}}"
        "function textOf(cell){return (cell&&cell.textContent?cell.textContent:'').trim();}"
        "function numericValue(text){var clean='';for(var i=0;i<text.length;i++){var ch=text.charAt(i);if((ch>='0'&&ch<='9')||ch==='.'||ch==='-'){clean+=ch;}}var value=parseFloat(clean);return isNaN(value)?null:value;}"
        "function comparable(cell,type){var raw=(cell&&cell.getAttribute)?cell.getAttribute('data-sort-value'):null;var text=(raw!==null&&raw!=='')?raw:textOf(cell);if(type==='number'){var value=numericValue(text);return {empty:value===null,value:value===null?0:value};}return {empty:text.length===0,value:text.toLowerCase()};}"
        "function clearSort(table){var headers=table.querySelectorAll('th.sortAsc,th.sortDesc');for(var i=0;i<headers.length;i++){headers[i].classList.remove('sortAsc');headers[i].classList.remove('sortDesc');}}"
        "function sortTable(th){var table=th.closest('table');if(!table||!table.tBodies.length){return;}var body=table.tBodies[0];var column=th.cellIndex;var type=th.getAttribute('data-sort-type')||'text';var dir=th.classList.contains('sortAsc')?'desc':'asc';var rows=Array.prototype.slice.call(body.rows).filter(function(row){return row.cells.length>column&&!row.cells[0].hasAttribute('colspan');}).map(function(row,index){return {row:row,index:index};});"
        "rows.sort(function(a,b){var av=comparable(a.row.cells[column],type);var bv=comparable(b.row.cells[column],type);if(av.empty!==bv.empty){return av.empty?1:-1;}var cmp=0;if(type==='number'){cmp=av.value-bv.value;}else{cmp=av.value.localeCompare(bv.value,undefined,{numeric:true,sensitivity:'base'});}if(cmp===0){cmp=a.index-b.index;}return dir==='asc'?cmp:-cmp;});"
        "clearSort(table);th.classList.add(dir==='asc'?'sortAsc':'sortDesc');for(var i=0;i<rows.length;i++){body.appendChild(rows[i].row);}var scroller=table.closest('.rosterTableWrap');if(scroller&&scroller.kboOotpUpdate){scroller.kboOotpUpdate();}}"
        "function makeButton(cls){var button=document.createElement('button');button.type='button';button.className='kboOotpScrollButton '+cls;button.tabIndex=-1;return button;}"
        "function installOotpScrollbar(scroller){if(!scroller||scroller.getAttribute('data-kbo-scrollbar')==='1'){return;}var host=scroller.parentElement;if(!host){return;}scroller.setAttribute('data-kbo-scrollbar','1');scroller.classList.add('kboCustomScroll');host.classList.add('kboScrollHost');if(scroller.classList.contains('rosterTableWrap')){host.classList.add('kboRosterScrollHost');}var bar=document.createElement('div');bar.className='kboOotpScrollbar';if(scroller.classList.contains('dropdown')||scroller.classList.contains('faFilterMenu')||scroller.classList.contains('ootpChoiceMenu')){bar.classList.add('kboDropdownScrollbar');}var less=makeButton('less');var track=document.createElement('div');track.className='kboOotpScrollTrack';var thumb=document.createElement('div');thumb.className='kboOotpScrollThumb';var more=makeButton('more');track.appendChild(thumb);bar.appendChild(less);bar.appendChild(track);bar.appendChild(more);host.appendChild(bar);var dragging=false;var dragStartY=0;var dragStartTop=0;"
        "function layoutBar(){var hr=host.getBoundingClientRect();var sr=scroller.getBoundingClientRect();bar.style.top=Math.round(sr.top-hr.top)+'px';bar.style.height=Math.round(sr.height)+'px';bar.style.right=Math.round(hr.right-sr.right)+'px';}"
        "function metrics(){layoutBar();var maxScroll=scroller.scrollHeight-scroller.clientHeight;var trackH=track.clientHeight;var buttonH=less.offsetHeight||20;var thumbH=maxScroll>1?Math.max(buttonH,Math.round(trackH*scroller.clientHeight/scroller.scrollHeight)):trackH;if(thumbH>trackH){thumbH=trackH;}var maxTop=Math.max(0,trackH-thumbH);var top=maxScroll>1?Math.round(maxTop*scroller.scrollTop/maxScroll):0;return {maxScroll:maxScroll,trackH:trackH,thumbH:thumbH,maxTop:maxTop,top:top};}"
        "function update(){bar.style.display='grid';var m=metrics();var visible=m.maxScroll>1&&m.trackH>0;scroller.classList.toggle('kboCustomScrollVisible',visible);bar.style.display=visible?'grid':'none';thumb.style.height=m.thumbH+'px';thumb.style.top=m.top+'px';}"
        "function scrollByAmount(direction,page){var step=page?Math.max(27,Math.floor(scroller.clientHeight*.85)):27;scroller.scrollTop+=direction*step;}"
        "less.addEventListener('click',function(e){e.preventDefault();scrollByAmount(-1,false);});more.addEventListener('click',function(e){e.preventDefault();scrollByAmount(1,false);});"
        "bar.addEventListener('wheel',function(e){e.preventDefault();scroller.scrollTop+=e.deltaY;},{passive:false});"
        "track.addEventListener('mousedown',function(e){if(e.target===thumb){return;}var rect=track.getBoundingClientRect();var m=metrics();var y=e.clientY-rect.top;if(y<m.top){scrollByAmount(-1,true);}else if(y>m.top+m.thumbH){scrollByAmount(1,true);}});"
        "thumb.addEventListener('mousedown',function(e){e.preventDefault();dragging=true;thumb.classList.add('dragging');dragStartY=e.clientY;dragStartTop=metrics().top;document.body.style.userSelect='none';});"
        "document.addEventListener('mousemove',function(e){if(!dragging){return;}var m=metrics();if(m.maxTop<=0||m.maxScroll<=0){return;}var nextTop=Math.max(0,Math.min(m.maxTop,dragStartTop+(e.clientY-dragStartY)));scroller.scrollTop=nextTop*m.maxScroll/m.maxTop;});"
        "document.addEventListener('mouseup',function(){if(!dragging){return;}dragging=false;thumb.classList.remove('dragging');document.body.style.userSelect='';});"
        "scroller.addEventListener('scroll',update);window.addEventListener('resize',update);if(scroller.classList.contains('ootpChoiceMenu')||scroller.classList.contains('faFilterMenu')){var details=scroller.closest('details');if(details){details.addEventListener('toggle',function(){setTimeout(update,0);});}}scroller.kboOotpUpdate=update;setTimeout(update,0);setTimeout(update,150);}"
        "function makeConfirmButton(cls,icon,text){var button=document.createElement('button');button.type='button';button.className='ootpConfirmButton '+cls;var mark=document.createElement('span');mark.className='ootpConfirmButtonIcon';mark.textContent=icon;var label=document.createElement('span');label.textContent=text;button.appendChild(mark);button.appendChild(label);return button;}"
        "function formatConfirmQuestion(template,name){return template.indexOf('%%s')>=0?template.replace('%%s',name):template;}"
        "function installRightsConfirm(){var pendingHref='';var messageTemplate=");
    kbo_webview_append_js_string(buffer, confirm_message);
    kbo_window_text_appendf(
        buffer,
        ";if(document.getElementById('kboRightsConfirmOverlay')){return;}var overlay=document.createElement('div');overlay.id='kboRightsConfirmOverlay';overlay.className='ootpConfirmOverlay';var dialog=document.createElement('div');dialog.className='ootpConfirmDialog';var title=document.createElement('div');title.className='ootpConfirmTitle';var question=document.createElement('span');question.className='ootpConfirmQuestion';question.textContent='?';var titleText=document.createElement('span');titleText.className='ootpConfirmTitleText';titleText.textContent=");
    kbo_webview_append_js_string(buffer, confirm_title);
    kbo_window_text_appendf(
        buffer,
        ";title.appendChild(question);title.appendChild(titleText);var body=document.createElement('div');body.className='ootpConfirmBody';var message=document.createElement('div');message.className='ootpConfirmMessage';var actions=document.createElement('div');actions.className='ootpConfirmActions';var ok=makeConfirmButton('primary','\\u2713',");
    kbo_webview_append_js_string(buffer, confirm_ok);
    kbo_window_text_appendf(
        buffer,
        ");var cancel=makeConfirmButton('','\\u2715',");
    kbo_webview_append_js_string(buffer, confirm_cancel);
    kbo_window_text_appendf(
        buffer,
        ");actions.appendChild(ok);actions.appendChild(cancel);body.appendChild(message);dialog.appendChild(title);dialog.appendChild(body);dialog.appendChild(actions);overlay.appendChild(dialog);document.body.appendChild(overlay);"
        "function close(){overlay.classList.remove('show');pendingHref='';}"
        "function open(link){pendingHref=link.getAttribute('href')||'';var name=link.getAttribute('data-player')||");
    kbo_webview_append_js_string(buffer, confirm_player);
    kbo_window_text_appendf(
        buffer,
        ";message.textContent=formatConfirmQuestion(messageTemplate,name);overlay.classList.add('show');cancel.focus();}"
        "document.addEventListener('click',function(e){var node=e.target;while(node&&node!==document&&!(node.classList&&node.classList.contains('rightsRelease'))){node=node.parentNode;}if(!node||node===document){return;}e.preventDefault();open(node);});"
        "cancel.addEventListener('click',function(){close();});overlay.addEventListener('click',function(e){if(e.target===overlay){close();}});ok.addEventListener('click',function(){var href=pendingHref;close();if(href){window.location.href=href;}});document.addEventListener('keydown',function(e){if(e.key==='Escape'&&overlay.classList.contains('show')){close();}});}"
        "function closestPlayerHover(node){while(node&&node!==document){if(node.getAttribute&&node.getAttribute('data-kbo-player-hover')==='1'&&node.getAttribute('data-player-id')){return node;}node=node.parentNode;}return null;}"
        "function sendKboCommand(href){try{if(window.chrome&&window.chrome.webview&&window.chrome.webview.postMessage){window.chrome.webview.postMessage(href);return;}}catch(_){}try{window.location.href=href;}catch(_){}}"
        "function installPlayerHoverBridge(){if(window.__kboPlayerHoverBridgeInstalled){return;}window.__kboPlayerHoverBridgeInstalled=1;var active=null;var hoverTimer=0;var hideTimer=0;var lastHref='';var shownAt=0;var seq=0;window.__kboPlayerHoverSeq=0;function clearHoverTimer(){if(hoverTimer){clearTimeout(hoverTimer);hoverTimer=0;}}function clearHideTimer(){if(hideTimer){clearTimeout(hideTimer);hideTimer=0;}}function coordsFor(el){var r=el.getBoundingClientRect();var x=Math.round(r.left+Math.min(r.width,18));var y=Math.round(r.top+Math.min(r.height,18));return {x:Math.max(0,x),y:Math.max(0,y)};}function send(href){if(href===lastHref){return;}lastHref=href;sendKboCommand(href);}function show(el){if(!el||el!==active){return;}var id=el.getAttribute('data-player-id');if(!id){return;}var pt=coordsFor(el);shownAt=Date.now();seq++;window.__kboPlayerHoverSeq=seq;send('kbo://player-hover/show/'+encodeURIComponent(id)+'/'+pt.x+'/'+pt.y+'/'+seq);}function hideNow(el){clearHoverTimer();clearHideTimer();seq++;window.__kboPlayerHoverSeq=seq;if(!el){return;}var id=el.getAttribute('data-player-id');if(id){send('kbo://player-hover/hide/'+encodeURIComponent(id)+'/'+seq);}}function scheduleHide(el){clearHoverTimer();clearHideTimer();var wait=Math.max(180,520-(Date.now()-shownAt));hideTimer=setTimeout(function(){if(active===el){active=null;}hideNow(el);},wait);}document.addEventListener('mouseover',function(e){var el=closestPlayerHover(e.target);if(!el){return;}clearHideTimer();if(el===active){return;}if(active){hideNow(active);}active=el;clearHoverTimer();hoverTimer=setTimeout(function(){show(el);},180);},true);document.addEventListener('mouseout',function(e){var el=closestPlayerHover(e.target);if(!el||el!==active){return;}var to=e.relatedTarget;if(to&&el.contains(to)){return;}scheduleHide(el);},true);window.addEventListener('blur',function(){hideNow(active);active=null;});}"
        "installGameSurfaceGuards();"
        "installRightsConfirm();"
        "installPlayerHoverBridge();"
        "function installRenderEnhancements(){var headers=document.querySelectorAll('.ootpRosterTable th[data-sort-type]');for(var i=0;i<headers.length;i++){if(headers[i].getAttribute('data-kbo-sort-installed')==='1'){continue;}headers[i].setAttribute('data-kbo-sort-installed','1');headers[i].addEventListener('click',function(){sortTable(this);});}var customScrollers=document.querySelectorAll('.content,.card,.rosterTableWrap,.settingsCard,.modBuildCard,.dropdown,.faFilterMenu,.ootpChoiceMenu');for(var r=0;r<customScrollers.length;r++){installOotpScrollbar(customScrollers[r]);}}"
        "window.__kboHubAfterRender=installRenderEnhancements;"
        "installRenderEnhancements();"
        "})();"
    );
}

void kbo_webview_append_roster_sort_script(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<script>");
    kbo_webview_append_roster_sort_script_body(buffer);
    kbo_window_text_appendf(buffer, "</script>");
}
