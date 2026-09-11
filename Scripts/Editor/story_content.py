"""Graybox scene treatment adapted from the approved Aurelion contracts.

Dialogue is newly authored implementation text, not a claim to quote the manuscript.
Scene actions and canon remain owned by the mission/cinematic native contracts.
"""

# Beat, zone, controlled hero, readable scene lines. Authoring supplies actual placement/camera bindings.
SCENES = [
    ('MeetingAndCarrierRescue', 'Z05', 'Selene', [
        ('Tarrik', 'Hold fire. The carrier is falling into the terminal.'),
        ('Selene', 'Keep its weight off the bridge. I can hold the fracture long enough.'),
        ('Tarrik', 'Everyone aboard gets a route out. Dominion and Reformation.'),
        ('Carrier control', 'Rescue confirmed. Seven thousand, one hundred and eighty-four aboard. All accounted for.'),
        ('Selene', 'There is something else inside. We reach the survivors together.'),
    ]),
    ('FreeTrappedMarine', 'Z06', 'Tarrik', [
        ('Trapped marine', 'My leg is caught. The brace will not move.'),
        ('Tarrik', 'Keep still. I have the brace.'),
        ('Selene', 'The ceiling is held. Pull him clear now.'),
        ('Trapped marine', 'I am clear. Thank you.'),
    ]),
    ('GroundLyric', 'Z06', 'Tarrik', [
        ('Selene', 'Lyric. Follow my voice. Here, with us.'),
        ('Lyric', 'I can hear you. It is still there.'),
        ('Selene', 'I know. You do not have to hold this alone.'),
        ('Tarrik', 'She is alive. Get her to the refuge. This has not cured her.'),
    ]),
    ('DestroyDominionResonator', 'Z07', 'Tarrik', [
        ('Tarrik', 'That resonator is a cage. It goes no farther.'),
        ('Tharne', 'Command will ask what happened here.'),
        ('Tarrik', 'Tell them I destroyed it. No one is being handed back to it.'),
    ]),
    ('DestroyReformationCage', 'Z07', 'Selene', [
        ('Selene', 'Ours is not an answer either. Stand clear of the containment frame.'),
        ('Malik', 'The feed is isolated. You have the cage.'),
        ('Selene', 'Then break it. Neither side keeps one.'),
    ]),
    ('ShareIsolatedThreatData', 'Z07', 'Selene', [
        ('Tharne', 'A local threat feed only. It cannot reach either command network.'),
        ('Malik', 'My squad keeps its command authority.'),
        ('Selene', 'So does mine. Share what keeps these people alive.'),
        ('Lyessa', 'Two mixed groups need passage: stretchers west, walkers east. Choose who moves first.'),
    ]),
    ('SurvivorsClearAndQuarantine', 'Z08', 'Tarrik', [
        ('Lyessa', 'West stretchers are clear. The last marine is with me.'),
        ('Malik', 'East walkers clear. Dominion and Reformation, all through.'),
        ('Tarrik', 'Keep them moving. No one comes back into the crucible.'),
        ('Terminal', 'Quarantine sealed. Contrary bearers: proceed to recognition.'),
    ]),
    ('ContraryWitnessRecognized', 'Z09', 'Tarrik', [
        ('Terminal', 'Contrary witnesses recognized.'),
        ('Selene', 'It recognizes us. That does not mean we agree to what it asks.'),
        ('Tarrik', 'Then we hear it, and answer for ourselves.'),
    ]),
    ('TarrikIndependentAssent', 'Z10', 'Tarrik', [
        ('Terminal', 'First bearer: independent concurrence required.'),
        ('Tarrik', 'I assent to stabilizing containment. I do not authorize release.'),
        ('Terminal', 'First assent recorded. The contrary bearer must answer independently.'),
    ]),
    ('SeleneIndependentAssent', 'Z10', 'Selene', [
        ('Terminal', 'Contrary bearer: independent concurrence required.'),
        ('Selene', 'My answer is mine. Stabilize the boundary. Keep it closed.'),
        ('Terminal', 'Contrary assent recorded. No release has been granted.'),
    ]),
    ('MeridianContainment', 'Z10', 'Selene', [
        ('Terminal', 'Meridian complete. Containment stabilized. Terminal authority available.'),
        ('Tarrik', 'Authority is not permission. Withhold release.'),
        ('Selene', 'The boundary stays closed.'),
        ('Terminal', 'Release withheld. Crownmark Five integrated.'),
    ]),
    ('FifthWitness', 'Z10', 'Selene', [
        ('Fifth Witness', 'The King contained Eclipse, then entered the contained side as its reference.'),
        ('Selene', 'We are witnessing what happened. We are not changing it.'),
        ('Tarrik', 'And completing this terminal does not open the prison.'),
        ('Terminal', 'Historical witness retained by both bearers. Boundary closed.'),
    ]),
    ('GrammarPropagation', 'Z10', 'Selene', [
        ('Selene', 'The pattern is leaving the terminal. Stop the transmission.'),
        ('Terminal', 'Containment grammar propagation is already in progress.'),
        ('Tarrik', 'The interruption did not stop it.'),
        ('Selene', 'Then we carry that warning. Lyric is still alive, and what happened to her is still there.'),
    ]),
    ('VoluntaryStay', 'Z11', 'Selene', [
        ('Terminal', 'Quarantine no longer requires the bearers to remain together. Both departure routes are available.'),
        ('Tarrik', 'We can leave.'),
        ('Selene', 'I know. I would rather speak before we do.'),
        ('Tarrik', 'So would I.'),
    ]),
    ('CauldronRecorderReceived', 'Z11', 'Selene', [
        ('Tarrik', 'The Cauldron recorder. You should hear what it actually kept.'),
        ('Selene', 'I will keep the record with its source. No one gets to rewrite it for me.'),
        ('Tarrik', 'It is yours to carry.'),
    ]),
    ('Record7283Received', 'Z11', 'Tarrik', [
        ('Selene', 'Record 7283. Take it. Read it against what we saw here.'),
        ('Tarrik', 'I will. We need both records, and the warning between them.'),
        ('Selene', 'Keep the distinction between what it shows and what we think it means.'),
    ]),
    ('ContainmentPact', 'Z11', 'Tarrik', [
        ('Tarrik', 'No release without your direct, contrary concurrence.'),
        ('Selene', 'And none without yours. No proxy, no compromised bearer.'),
        ('Tarrik', 'No controlled aperture. No bypass dressed up as an exception.'),
        ('Selene', 'Agreed. Containment holds until we both answer, directly and freely.'),
    ]),
    ('SeparateDepartures', 'Z12', 'Tarrik', [
        ('Selene', 'My people need to hear this from me.'),
        ('Tarrik', 'Mine too. Separate routes. The same warning.'),
        ('Selene', 'Keep the boundary closed.'),
        ('Tarrik', 'Until we both answer.'),
    ]),
]

EVIDENCE = {
    'Aurelion_FifthWitness': dict(custodian='AurelionTerminal', summary='The Fifth Witness: historical containment.',
        text='Both bearers observe the historical containment of Eclipse. The King enters the contained side as the reference. Meridian completion stabilizes containment and enables terminal authority; it does not open the prison. The boundary remains closed and release remains withheld.'),
    'CauldronRecorder': dict(custodian='Tarrik', summary='The Cauldron recorder, entrusted to Selene.',
        text='Tarrik gives Selene the Cauldron recorder during their voluntary conversation after quarantine no longer compels proximity. The physical record and its provenance remain distinct from any interpretation of its contents.'),
    'Record7283': dict(custodian='Selene', summary='Record 7283, entrusted to Tarrik.',
        text='Selene gives Tarrik Record 7283 during the voluntary evidence exchange. Together the records and the Fifth Witness inform a shared warning: terminal authority does not authorize release, and direct contrary concurrence admits no proxy or bypass.'),
}
