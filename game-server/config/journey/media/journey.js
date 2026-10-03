(function(){
'use strict';
var state=null,selected='',busy=false,loadStarted=new Date().getTime();
function el(id){return document.getElementById(id);}
function hidden(id,value){if(value)el(id).setAttribute('hidden','hidden');else el(id).removeAttribute('hidden');}
function nativeControl(show){if(window.AionObject&&typeof window.AionObject.JourneyVisibility==='function')window.AionObject.JourneyVisibility(show?1:0);}
function notice(text){el('notice').textContent=text||'';}
function encode(fields){var parts=[],k;for(k in fields)if(fields.hasOwnProperty(k))parts.push(encodeURIComponent(k)+'='+encodeURIComponent(fields[k]));return parts.join('&');}
function token(){var m=/(?:[?&])session_id=([^&]*)/.exec(location.search);return m?decodeURIComponent(m[1]):'';}
function presentation(data){
 var j=data.journey;
 el('starter-title').textContent='Begin in '+j.starter;el('play').textContent='Play '+j.starter;
 el('capital-title').textContent=el('confirm').textContent='Ascend to '+j.capital;
 el('starter-description').textContent=j.description;el('quest-count').textContent=data.quests+' '+j.starter;
 el('ceremony-line').textContent='Complete the '+j.capital+' ceremony to earn its rewards';
 el('review-description').textContent='You will become a level 10 Daeva in '+j.capital+'. Your skipped quests will be completed, their rewards sent to your mailbox, and your class-specific Dispatch to '+j.onward+' quest added. A Ceremony in '+j.capital+' remains playable and grants its rewards when completed.';
 el('story-warning').textContent='This completes the '+j.starter+' story for this character.';
 el('starter-art').style.backgroundImage="url('/journey/media/"+j.starterArt+".jpg')";
 el('capital-art').style.backgroundImage="url('/journey/media/"+j.capitalArt+".jpg')";
}
function request(method,path,fields,done,timeout){
 var xhr=new XMLHttpRequest(),settled=false,timer;timeout=timeout||20000;
 function finish(status,data){if(settled)return;settled=true;clearTimeout(timer);done(status,data);}
 function expired(){if(settled)return;finish(0,{error:'The request timed out. Reopen the menu to check your saved choice.'});xhr.abort();}
 xhr.open(method,path,true);xhr.timeout=timeout;
 if(method==='POST')xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
 xhr.onreadystatechange=function(){if(xhr.readyState!==4||settled)return;var data;try{data=JSON.parse(xhr.responseText);}catch(e){data={error:'The journey menu could not connect. Reopen it to try again.'};}finish(xhr.status,data);};
 xhr.ontimeout=expired;
 // Older client WebKit builds can ignore XMLHttpRequest.timeout.
 timer=setTimeout(expired,timeout);
 xhr.send(method==='POST'?encode(fields):null);
}
function load(){
 var remaining=12000-(new Date().getTime()-loadStarted);
 if(remaining<=0){notice('The journey menu could not connect. Close it and reopen Choose Your Journey.');return;}
 request('GET','/journey/state?session_id='+encodeURIComponent(token()),{},function(status,data){
  if(status!==200){if(new Date().getTime()-loadStarted<12000){setTimeout(load,300);return;}notice('The journey menu could not connect. Close it and reopen Choose Your Journey.');return;}
  state=data;presentation(data);var j=data.journey;
  if(!data.eligible){hidden('paths',true);hidden('complete',false);el('complete-title').textContent=data.welcome?'Welcome to '+j.capital:'Your journey is underway';el('receipt').textContent=data.decision==='SKIP'?'Your '+j.starter+' skip has already been applied. Collect your skipped quest rewards from the mailbox. Speak to '+j.guide+' for A Ceremony in '+j.capital+'. '+(data.ceremonyRewardsMailed?'Its rewards were already included in your earlier mail bundle.':'Complete the ceremony to earn its rewards.')+' Then see '+j.travelGuide+' for Dispatch to '+j.onward+'.':'This choice is available to starting-class characters in '+j.starter+', before Ascension.';if(data.welcome)nativeControl(true);return;}
  el('greeting').textContent=data.name+', how will your story begin?';
  if(data.prompt)nativeControl(true);
 },Math.min(2500,remaining));
}
function choose(choice){
 if(busy||!state)return;busy=true;el('play').disabled=el('confirm').disabled=true;notice('Preparing your journeyâ€¦');
 request('POST','/journey/action',{session_id:token(),request:state.request,choice:choice,'class':selected},function(status,data){
  busy=false;el('play').disabled=el('confirm').disabled=false;
  if(status!==200){notice(data.error);return;}
  state=data;presentation(data);hidden('paths',true);hidden('classes',true);hidden('complete',false);notice('');
  el('complete-title').textContent=choice==='skip'?'Welcome to '+data.journey.capital:'Your story begins';el('receipt').textContent=data.notice;
  if(choice==='play'||choice==='ack')nativeControl(false);else if(data.welcome)nativeControl(true);
 });
}
el('play').onclick=function(){choose('play');};
el('skip').onclick=function(){if(!state)return;hidden('paths',true);hidden('classes',false);notice('');var list=el('class-list');list.innerHTML='';
 for(var i=0;i<state.classes.length;i++)(function(c){var b=document.createElement('button');b.textContent=c.name;b.onclick=function(){selected=c.id;var buttons=list.getElementsByTagName('button');for(var j=0;j<buttons.length;j++)buttons[j].className='';b.className='selected';el('selected-name').textContent=c.name;hidden('review',false);};list.appendChild(b);})(state.classes[i]);
};
el('back').onclick=function(){if(busy)return;hidden('classes',true);hidden('review',true);hidden('paths',false);selected='';};
el('confirm').onclick=function(){if(selected)choose('skip');};el('close').onclick=function(){if(state&&state.welcome)choose('ack');else nativeControl(false);};
load();
})();
